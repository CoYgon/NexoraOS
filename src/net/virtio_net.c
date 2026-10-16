#include "virtio_net.h"
#include "../serial.h"
#include "../limine.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "netif/ethernet.h"
#include "lwip/err.h"

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC
#define VIRTIO_VENDOR 0x1AF4
#define VIRTIO_NET_LEGACY 0x1000
#define VIRTIO_QUEUE_SIZE 256
#define VIRTIO_PAGE_SIZE 4096
#define RX_BUFFER_SIZE 2048
#define VIRTIO_STATUS_ACK 1
#define VIRTIO_STATUS_DRIVER 2
#define VIRTIO_STATUS_DRIVER_OK 4
#define VIRTIO_STATUS_FAILED 128
#define VIRTIO_NET_F_MAC (1u << 5)

static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline uint8_t inb(uint16_t p) { uint8_t v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline void outw(uint16_t p, uint16_t v) { __asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p)); }
static inline uint16_t inw(uint16_t p) { uint16_t v; __asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline void outl(uint16_t p, uint32_t v) { __asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p)); }
static inline uint32_t inl(uint16_t p) { uint32_t v; __asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p)); return v; }

struct vring_desc { uint64_t addr; uint32_t len; uint16_t flags; uint16_t next; };
struct vring_avail { uint16_t flags, idx; uint16_t ring[VIRTIO_QUEUE_SIZE]; };
struct vring_used_elem { uint32_t id, len; };
struct vring_used { uint16_t flags, idx; struct vring_used_elem ring[VIRTIO_QUEUE_SIZE]; };
struct __attribute__((aligned(4096))) virtqueue_page { uint8_t bytes[12288]; };
static struct virtqueue_page rxq_page, txq_page;
static uint8_t rx_buffers[VIRTIO_QUEUE_SIZE][RX_BUFFER_SIZE] __attribute__((aligned(16)));
static uint8_t tx_buffer[RX_BUFFER_SIZE] __attribute__((aligned(16)));
static struct netif *g_netif;
static uint16_t g_io;
static uint8_t g_mac[6];
static int g_ready;
static uint16_t rx_last_used, tx_last_used;
static uint16_t g_rx_size, g_tx_size;
static uint32_t g_features;
static uint64_t g_rx_packets, g_tx_packets, g_rx_drops, g_tx_errors;

static volatile struct limine_kernel_address_response *g_kernel_address;
void virtio_net_set_kernel_address(volatile struct limine_kernel_address_response *r) { g_kernel_address = r; }
static uint64_t phys(const void *p) {
    uintptr_t v=(uintptr_t)p;
    if (!g_kernel_address || v < g_kernel_address->virtual_base) return 0;
    return (uint64_t)(v-g_kernel_address->virtual_base)+g_kernel_address->physical_base;
}
static uint32_t pci_read(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t reg) {
    uint32_t addr=0x80000000u|((uint32_t)bus<<16)|((uint32_t)dev<<11)|((uint32_t)fn<<8)|(reg&0xFCu);
    outl(PCI_CONFIG_ADDR,addr); return inl(PCI_CONFIG_DATA);
}
static void pci_write(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t reg,uint32_t value) {
    uint32_t addr=0x80000000u|((uint32_t)bus<<16)|((uint32_t)dev<<11)|((uint32_t)fn<<8)|(reg&0xFCu);
    outl(PCI_CONFIG_ADDR,addr); outl(PCI_CONFIG_DATA,value);
}
static int find_device(uint16_t vendor,uint16_t device,uint8_t *ob,uint8_t *od,uint8_t *of) {
    for(uint16_t b=0;b<256;b++) for(uint8_t d=0;d<32;d++) {
        uint32_t id=pci_read((uint8_t)b,d,0,0); if((uint16_t)id==0xFFFF) continue;
        uint8_t functions=((pci_read((uint8_t)b,d,0,0x0C)>>16)&0x80)?8:1;
        for(uint8_t f=0;f<functions;f++) { id=pci_read((uint8_t)b,d,f,0); if((uint16_t)id==vendor && (uint16_t)(id>>16)==device) { *ob=(uint8_t)b;*od=d;*of=f;return 1; } }
    }
    return 0;
}
static struct vring_desc *descs(struct virtqueue_page *q) { return (struct vring_desc *)q->bytes; }
static struct vring_avail *avail(struct virtqueue_page *q) { return (struct vring_avail *)(q->bytes+sizeof(struct vring_desc)*VIRTIO_QUEUE_SIZE); }
static volatile struct vring_used *used(struct virtqueue_page *q) { return (volatile struct vring_used *)(q->bytes+2u*VIRTIO_PAGE_SIZE); }
static int queue_setup(uint16_t queue,struct virtqueue_page *page,uint16_t *size_out) {
    outw(g_io+0x0E,queue); uint16_t qsize=inw(g_io+0x0C);
    if (qsize != VIRTIO_QUEUE_SIZE) return 0;
    uint64_t page_phys=phys(page);
    if (!page_phys || (page_phys&4095u) || (page_phys>>12)>0xFFFFFFFFu) return 0;
    outl(g_io+0x08,(uint32_t)(phys(page)>>12)); *size_out=qsize; return 1;
}
static void set_status(uint8_t status) { outb(g_io+0x12,status); }
int virtio_net_init(struct netif *netif) {
    uint8_t bus,dev,fn; uint16_t qsz;
    if (!netif || !g_kernel_address) { serial_write("[net] kernel physical address unavailable\n"); return 0; }
    if (!find_device(VIRTIO_VENDOR,VIRTIO_NET_LEGACY,&bus,&dev,&fn)) { serial_write("[net] legacy VirtIO-net PCI device not found\n"); return 0; }
    uint32_t command=pci_read(bus,dev,fn,0x04); pci_write(bus,dev,fn,0x04,command|0x5u);
    uint32_t bar=pci_read(bus,dev,fn,0x10); if (!(bar&1u)) { serial_write("[net] VirtIO BAR is not I/O mapped; use disable-modern=on\n"); return 0; }
    g_io=(uint16_t)(bar&~3u); if (!g_io) return 0;
    set_status(0); set_status(VIRTIO_STATUS_ACK); set_status(VIRTIO_STATUS_ACK|VIRTIO_STATUS_DRIVER);
    g_features=inl(g_io+0x00); uint32_t guest=(g_features&VIRTIO_NET_F_MAC)?VIRTIO_NET_F_MAC:0; outl(g_io+0x04,guest);
    memset(&rxq_page,0,sizeof(rxq_page)); memset(&txq_page,0,sizeof(txq_page));
    if (!queue_setup(0,&rxq_page,&qsz)) { set_status(VIRTIO_STATUS_FAILED); return 0; }
    g_rx_size=qsz;
    if (!queue_setup(1,&txq_page,&qsz)) { set_status(VIRTIO_STATUS_FAILED); return 0; }
    g_tx_size=qsz;
    for(uint16_t i=0;i<VIRTIO_QUEUE_SIZE;i++) {
        uint64_t rx_phys=phys(rx_buffers[i]);
        if (!rx_phys || rx_phys+RX_BUFFER_SIZE-1 < rx_phys) { set_status(VIRTIO_STATUS_FAILED); return 0; }
        descs(&rxq_page)[i].addr=rx_phys; descs(&rxq_page)[i].len=RX_BUFFER_SIZE;
        descs(&rxq_page)[i].flags=2; descs(&rxq_page)[i].next=0;
        if(i<g_rx_size) avail(&rxq_page)->ring[i]=i;
    }
    avail(&rxq_page)->idx=g_rx_size;
    set_status(VIRTIO_STATUS_ACK|VIRTIO_STATUS_DRIVER|VIRTIO_STATUS_DRIVER_OK);
    for(int i=0;i<6;i++) g_mac[i]=inb(g_io+0x14+i);
    g_netif=netif; rx_last_used=0; tx_last_used=0; g_ready=1;
    __sync_synchronize(); outw(g_io+0x10,0); /* Notify RX queue. */
    serial_write("[net] VirtIO-net ready, MAC ");
    for(int i=0;i<6;i++) { serial_write_hex(g_mac[i]); if(i!=5) serial_write(":"); }
    serial_write("\n");
    return 1;
}
int virtio_net_poll(void) {
    if (!g_ready || !g_netif) return 0;
    int packets=0;
    volatile struct vring_used *u=used(&rxq_page);
    __sync_synchronize();
    while(rx_last_used!=u->idx) {
        uint16_t slot=rx_last_used%g_rx_size;
        uint32_t id=u->ring[slot].id, len=u->ring[slot].len; rx_last_used++;
        if(id>=g_rx_size) {
            g_rx_drops++;
            serial_write("[net] VirtIO RX returned an invalid descriptor id; disabling NIC\n");
            g_ready=0;
            return packets;
        }
        if(len>10 && len<=RX_BUFFER_SIZE) {
            /* No offloads are negotiated: legacy virtio-net header is 10 bytes. */
            struct pbuf *p=pbuf_alloc(PBUF_RAW,(u16_t)(len-10),PBUF_POOL);
            if(p) {
                if(pbuf_take(p,rx_buffers[id]+10,(u16_t)(len-10))==ERR_OK) {
                    if(g_netif->input(p,g_netif)!=ERR_OK) { pbuf_free(p); g_rx_drops++; }
                    else g_rx_packets++;
                } else { pbuf_free(p); g_rx_drops++; }
            }
            packets++;
        } else {
            g_rx_drops++;
            serial_write("[net] Dropped malformed VirtIO RX frame\n");
        }
        /* Recycle the descriptor even when the packet was malformed or pbuf allocation failed. */
        descs(&rxq_page)[id].addr=phys(rx_buffers[id]); descs(&rxq_page)[id].len=RX_BUFFER_SIZE; descs(&rxq_page)[id].flags=2;
        avail(&rxq_page)->ring[avail(&rxq_page)->idx%g_rx_size]=(uint16_t)id; avail(&rxq_page)->idx++; __sync_synchronize();
        outw(g_io+0x10,0);
    }
    return packets;
}
int virtio_net_is_ready(void) { return g_ready; }
const uint8_t *virtio_net_mac(void) { return g_mac; }

/* Called by lwIP netif linkoutput. */
err_t virtio_net_linkoutput(struct netif *netif, struct pbuf *p) {
    (void)netif; if(!g_ready || !p) return ERR_IF;
    if ((uint32_t)p->tot_len+10u>sizeof(tx_buffer)) return ERR_BUF;
    if (!phys(tx_buffer)) return ERR_IF;
    memset(tx_buffer,0,10); if(pbuf_copy_partial(p,tx_buffer+10,p->tot_len,0)!=p->tot_len) return ERR_BUF;
    struct vring_desc *d=descs(&txq_page); d[0].addr=phys(tx_buffer); d[0].len=p->tot_len+10; d[0].flags=0; d[0].next=0;
    avail(&txq_page)->ring[avail(&txq_page)->idx%g_tx_size]=0; avail(&txq_page)->idx++; __sync_synchronize();
    outw(g_io+0x10,1); uint32_t spin=1000000; while(used(&txq_page)->idx==tx_last_used && --spin) { }
    if(used(&txq_page)->idx!=tx_last_used) { tx_last_used++; g_tx_packets++; }
    else { g_tx_errors++; return ERR_IF; }
    return ERR_OK;
}

uint64_t virtio_net_rx_packets(void) { return g_rx_packets; }
uint64_t virtio_net_tx_packets(void) { return g_tx_packets; }
uint64_t virtio_net_rx_drops(void) { return g_rx_drops; }
uint64_t virtio_net_tx_errors(void) { return g_tx_errors; }
