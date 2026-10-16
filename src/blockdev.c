#include "blockdev.h"
#include "serial.h"

static struct blockdev_info g_info;
static blockdev_read_fn g_read;
static blockdev_write_fn g_write;
static void *g_context;

static void str_copy(char *d, size_t cap, const char *s) {
    size_t i=0; if (!cap) return; while(s && s[i] && i+1<cap) { d[i]=s[i]; ++i; } d[i]=0;
}

int blockdev_register_rw(const char *name, uint64_t sectors, blockdev_read_fn read_fn, blockdev_write_fn write_fn, void *context) {
    if (!name || !*name || !sectors || !read_fn) return 0;
    g_info.present=1; g_info.sectors=sectors;
    str_copy(g_info.name,sizeof(g_info.name),name);
    g_info.model[0]=0; g_read=read_fn; g_write=write_fn; g_context=context;
    return 1;
}
int blockdev_register(const char *name, uint64_t sectors, blockdev_read_fn read_fn, void *context) {
    return blockdev_register_rw(name, sectors, read_fn, 0, context);
}

int blockdev_read(uint64_t lba, uint32_t count, void *buffer) {
    if (!g_info.present || !g_read || !buffer || !count) return 0;
    if (lba >= g_info.sectors || (uint64_t)count > g_info.sectors-lba) return 0;
    return g_read(g_context,lba,count,buffer);
}
int blockdev_write(uint64_t lba, uint32_t count, const void *buffer) {
    if (!g_info.present || !g_write || !buffer || !count) return 0;
    if (lba >= g_info.sectors || (uint64_t)count > g_info.sectors-lba) return 0;
    return g_write(g_context, lba, count, buffer);
}
int blockdev_is_writable(void) { return g_info.present && g_write; }
const struct blockdev_info *blockdev_get_info(void) { return &g_info; }
int blockdev_is_ready(void) { return g_info.present && g_read; }

#ifndef NEXORAOS_HOST_TEST
static inline void outb(uint16_t p,uint8_t v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline uint8_t inb(uint16_t p){uint8_t v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
static inline void insw(uint16_t p, void *dst, unsigned words){__asm__ volatile("cld; rep insw":"+D"(dst),"+c"(words):"d"(p):"memory");}
static void ata_delay(void){(void)inb(0x3F6);(void)inb(0x3F6);(void)inb(0x3F6);(void)inb(0x3F6);}
static int ata_wait(int want_drq) {
    for (unsigned i=0;i<1000000u;++i) {
        uint8_t s=inb(0x1F7);
        if (s==0xFF || s==0x00) return 0;
        if (s&0x01) return 0;
        if (!(s&0x80) && (!want_drq || (s&0x08))) return 1;
    }
    return 0;
}
static int ata_identify(uint8_t drive, uint16_t *words) {
    outb(0x1F6,(uint8_t)(0xA0u | (drive<<4))); ata_delay();
    outb(0x1F2,0); outb(0x1F3,0); outb(0x1F4,0); outb(0x1F5,0); outb(0x1F7,0xEC);
    uint8_t s=inb(0x1F7); if (s==0 || s==0xFF) return 0;
    if (!ata_wait(1)) return 0;
    insw(0x1F0,words,256); return 1;
}
static int ata_read(void *context,uint64_t lba,uint32_t count,void *buffer) {
    uint8_t drive=(uint8_t)(uintptr_t)context;
    uint8_t *dst=(uint8_t *)buffer;
    if (lba >= 0x10000000ULL || (uint64_t)count > 0x10000000ULL-lba) return 0;
    for(uint32_t n=0;n<count;++n,++lba,dst+=512) {
        if(!ata_wait(0)) return 0;
        outb(0x1F6,(uint8_t)(0xE0u|(drive<<4)|((lba>>24)&0x0F)));
        outb(0x1F2,1); outb(0x1F3,(uint8_t)lba); outb(0x1F4,(uint8_t)(lba>>8)); outb(0x1F5,(uint8_t)(lba>>16));
        outb(0x1F7,0x20); if(!ata_wait(1)) return 0;
        insw(0x1F0,dst,256); ata_delay();
    }
    return 1;
}
static void ata_probe(void) {
    uint16_t id[256];
    for(uint8_t drive=0;drive<2;++drive) {
        if(!ata_identify(drive,id)) continue;
        uint64_t sectors=((uint64_t)id[61]<<16)|id[60];
        if (sectors > 0x10000000ULL) sectors = 0x10000000ULL; /* driver is LBA28-only */
        if(!sectors) continue;
        if(blockdev_register(drive?"ata-primary-slave":"ata-primary-master",sectors,ata_read,(void *)(uintptr_t)drive)) {
            size_t p=0;
            for(unsigned w=27;w<=46 && p+1<sizeof(g_info.model);++w) {
                g_info.model[p++]=(char)(id[w]>>8); g_info.model[p++]=(char)(id[w]&0xFF);
            }
            while(p && g_info.model[p-1]==' ') --p; g_info.model[p]=0;
            serial_write("[block] detected "); serial_write(g_info.name); serial_write(" sectors="); serial_write_dec(sectors); serial_write(" model="); serial_write(g_info.model); serial_write("\n");
            return; /* One read-only device is exposed in this development stage. */
        }
    }
}
#endif

void blockdev_init(void) {
    g_info.present=0; g_info.sectors=0; g_info.name[0]=0; g_info.model[0]=0; g_read=0; g_write=0; g_context=0;
#ifndef NEXORAOS_HOST_TEST
    /* Legacy primary-channel ATA PIO only. This driver never writes sectors. */
    ata_probe();
#endif
}
