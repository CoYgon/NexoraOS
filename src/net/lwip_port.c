#include "lwip_port.h"
#include "virtio_net.h"
#include "../serial.h"
#include "../limine.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "lwip/init.h"
#include "lwip/netif.h"
#include "netif/ethernet.h"
#include "lwip/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/timeouts.h"
#include "lwip/ip4_addr.h"
#include "lwip/dns.h"
#include "lwip/tcp.h"
#include "lwip/sys.h"

static struct netif g_netif;
static int g_initialized;
static int g_dns_probe_started;
static int g_tcp_probe_started;
static uint64_t g_tsc_hz;
static uint64_t g_last_tsc;
static uint32_t g_elapsed_ms;
static int g_rdrand_checked;
static int g_rdrand_available;
static uint32_t g_last_link_state;
static uint32_t g_browser_started_ms;
static uint32_t g_browser_timeout_ms = 15000u;
static int g_rng_warning_logged;
static void browser_set_message(const char *s);
static int browser_state;
static struct tcp_pcb *browser_pcb;

static inline void outb(uint16_t p,uint8_t v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline uint8_t inb(uint16_t p){uint8_t v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
static inline uint64_t rdtsc(void){uint32_t lo,hi;__asm__ volatile("rdtsc":"=a"(lo),"=d"(hi));return ((uint64_t)hi<<32)|lo;}
static uint32_t cpuid_ecx(uint32_t leaf, uint32_t subleaf) {
    uint32_t a=leaf, b=0, c=subleaf, d=0;
    __asm__ volatile("cpuid" : "+a"(a), "=b"(b), "+c"(c), "=d"(d));
    return c;
}
static int hardware_rng_available(void) {
    if (!g_rdrand_checked) {
        g_rdrand_available = (cpuid_ecx(1,0) & (1u<<30)) != 0;
        g_rdrand_checked = 1;
    }
    return g_rdrand_available;
}
int nexora_secure_random_available(void) { return hardware_rng_available(); }

int nexora_secure_random(void *out, size_t len) {
    uint8_t *p=(uint8_t *)out;
    if ((!p && len) || !hardware_rng_available()) return 0;
    while (len) {
        uint64_t value=0;
        unsigned char ok=0;
        /* RDRAND is hardware-backed. Bounded retries avoid hanging on a failed source. */
        for (unsigned int retry=0; retry<10 && !ok; retry++)
            __asm__ volatile("rdrand %0; setc %1" : "=r"(value), "=qm"(ok));
        if (!ok) return 0;
        size_t take=len<sizeof(value)?len:sizeof(value);
        for (size_t i=0;i<take;i++) p[i]=(uint8_t)(value>>(i*8));
        p+=take; len-=take;
    }
    return 1;
}
uint32_t nexora_lwip_rand(void) {
    uint32_t value=0;
    if (nexora_secure_random(&value,sizeof(value))) return value;
    if (!g_rng_warning_logged) {
        serial_write("[rng] RDRAND unavailable/failed; secure random requests fail closed\n");
        g_rng_warning_logged=1;
    }
    /* No timestamp/XOR or PRNG fallback: it would falsely imply cryptographic entropy. */
    return 0;
}
static int pit_wait_10ms(void) {
    /* PIT channel 2 one-shot: 11932 ticks ~= 10 ms. */
    uint8_t v=inb(0x61); outb(0x61,(uint8_t)(v&~1u));
    outb(0x43,0xB0); outb(0x42,(uint8_t)(11932&0xFF)); outb(0x42,(uint8_t)(11932>>8));
    outb(0x61,(uint8_t)((v&~2u)|1u));
    int saw_low=0;
    for(uint32_t spin=0;spin<20000000u;spin++) {
        if(!(inb(0x61)&0x20u)) { saw_low=1; break; }
    }
    if(!saw_low) return 0;
    for(uint32_t spin=0;spin<20000000u;spin++) if(inb(0x61)&0x20u) return 1;
    return 0;
}
static void calibrate_tsc(void) {
    uint64_t a=rdtsc(); int valid=1;
    for(int i=0;i<5;i++) if(!pit_wait_10ms()) { valid=0; break; }
    uint64_t b=rdtsc();
    if(valid && b>a) { uint64_t hz=(b-a)*20u; if(hz>=1000000u && hz<=10000000000ull) g_tsc_hz=hz; }
    if(!g_tsc_hz) g_tsc_hz=1000000000ull; /* fallback only if PIT calibration is unavailable */
    g_last_tsc=rdtsc(); g_elapsed_ms=0;
}

u32_t sys_now(void) {
    uint64_t now=rdtsc();
    if(!g_tsc_hz) { g_last_tsc=now; return g_elapsed_ms; }
    if(now>=g_last_tsc) {
        uint64_t delta=now-g_last_tsc;
        uint64_t ms=(delta*1000u)/g_tsc_hz;
        if(ms) { g_elapsed_ms+=(uint32_t)ms; g_last_tsc+=(ms*g_tsc_hz)/1000u; }
    } else g_last_tsc=now;
    return g_elapsed_ms;
}

static err_t low_level_init(struct netif *netif) {
    const uint8_t *mac=virtio_net_mac();
    netif->hwaddr_len=6; memcpy(netif->hwaddr,mac,6);
    netif->mtu=1500;
    netif->flags=NETIF_FLAG_BROADCAST|NETIF_FLAG_ETHARP|NETIF_FLAG_LINK_UP;
    netif->name[0]='n'; netif->name[1]='x';
    return ERR_OK;
}
static err_t netif_init_cb(struct netif *netif) {
    netif->output=etharp_output;
    netif->linkoutput=virtio_net_linkoutput;
    return low_level_init(netif);
}
static void status_cb(struct netif *netif) {
    const ip4_addr_t *a=netif_ip4_addr(netif);
    serial_write("[net] IPv4 address: ");
    serial_write_dec(ip4_addr1(a)); serial_write("."); serial_write_dec(ip4_addr2(a)); serial_write(".");
    serial_write_dec(ip4_addr3(a)); serial_write("."); serial_write_dec(ip4_addr4(a)); serial_write("\n");
}
static err_t http_probe_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    (void)arg; (void)err;
    if (!p) {
        serial_write("[net] HTTP probe peer closed the connection\n");
        if (tcp_close(pcb)!=ERR_OK) { tcp_abort(pcb); return ERR_ABRT; }
        return ERR_OK;
    }
    serial_write("[net] TCP/HTTP probe received response bytes: ");
    serial_write_dec(p->tot_len);
    serial_write("\n");
    tcp_recved(pcb,p->tot_len);
    pbuf_free(p);
    tcp_recv(pcb,0);
    if (tcp_close(pcb)!=ERR_OK) { tcp_abort(pcb); return ERR_ABRT; }
    return ERR_OK;
}
static void http_probe_error(void *arg, err_t err) {
    (void)arg;
    serial_write("[net] HTTP probe TCP error: ");
    serial_write_dec((unsigned long long)(uint8_t)(-err));
    serial_write("\n");
}
static err_t http_probe_connected(void *arg, struct tcp_pcb *pcb, err_t err) {
    (void)arg;
    if (err!=ERR_OK) { serial_write("[net] HTTP probe TCP connect failed\n"); return err; }
    static const char request[]="GET / HTTP/1.0\r\nHost: example.com\r\nConnection: close\r\n\r\n";
    tcp_recv(pcb,http_probe_recv);
    err_t e=tcp_write(pcb,request,sizeof(request)-1,TCP_WRITE_FLAG_COPY);
    if (e==ERR_OK) { tcp_output(pcb); serial_write("[net] TCP connected to example.com:80; HTTP request sent\n"); }
    else serial_write("[net] HTTP request send failed\n");
    return e;
}
static void start_http_probe(const ip_addr_t *addr) {
    if (!addr || ip_addr_isany(addr) || g_tcp_probe_started) return;
    g_tcp_probe_started=1;
    struct tcp_pcb *pcb=tcp_new();
    if (!pcb) { serial_write("[net] HTTP probe could not allocate TCP PCB\n"); return; }
    tcp_err(pcb,http_probe_error);
    err_t e=tcp_connect(pcb,addr,80,http_probe_connected);
    if (e!=ERR_OK) { serial_write("[net] HTTP probe connect request failed\n"); tcp_abort(pcb); }
}

static void dns_probe_done(const char *name, const ip_addr_t *addr, void *arg) {
    (void)arg;
    if (!addr || ip_addr_isany(addr)) { serial_write("[net] DNS probe failed for "); serial_write(name); serial_write("\n"); return; }
    const ip4_addr_t *a=ip_2_ip4(addr);
    serial_write("[net] DNS OK: "); serial_write(name); serial_write(" -> ");
    serial_write_dec(ip4_addr1(a)); serial_write("."); serial_write_dec(ip4_addr2(a)); serial_write(".");
    serial_write_dec(ip4_addr3(a)); serial_write("."); serial_write_dec(ip4_addr4(a)); serial_write("\n");
    start_http_probe(addr);
}
void nexora_lwip_set_kernel_address(volatile struct limine_kernel_address_response *r) { virtio_net_set_kernel_address(r); }
int nexora_lwip_init(void) {
    if(g_initialized) return 1;
    calibrate_tsc();
    lwip_init();
    if(!virtio_net_init(&g_netif)) { serial_write("[net] NIC initialization failed; check QEMU virtio-net configuration\n"); return 0; }
    ip4_addr_t ip,mask,gw; IP4_ADDR(&ip,0,0,0,0); IP4_ADDR(&mask,0,0,0,0); IP4_ADDR(&gw,0,0,0,0);
    if(!netif_add(&g_netif,&ip,&mask,&gw,0,netif_init_cb,ethernet_input)) { serial_write("[net] lwIP netif_add failed\n"); return 0; }
    netif_set_default(&g_netif); netif_set_status_callback(&g_netif,status_cb);
    netif_set_hostname(&g_netif,"NexoraOS"); netif_set_up(&g_netif); netif_set_link_up(&g_netif);
    err_t e=dhcp_start(&g_netif);
    if(e!=ERR_OK) { serial_write("[net] DHCP start failed: "); serial_write_dec((unsigned long long)(uint8_t)(-e)); serial_write("\n"); }
    g_initialized=1;
    serial_write("[net] lwIP initialized; DHCP discovery started\n");
    return 1;
}
void nexora_lwip_poll(void) {
    if(!g_initialized) return;
    (void)virtio_net_poll();
    sys_check_timeouts();
    uint32_t now=sys_now();
    int link=nexora_lwip_link_ready();
    if ((uint32_t)link != g_last_link_state) {
        g_last_link_state=(uint32_t)link;
        serial_write(link ? "[net] IPv4 connectivity became ready\n" : "[net] IPv4 lease/link lost; waiting for DHCP\n");
        if (!link) {
            g_dns_probe_started=0;
            g_tcp_probe_started=0;
            if (browser_pcb) { tcp_abort(browser_pcb); browser_pcb=0; }
            if (browser_state==1) { browser_state=-1; browser_set_message("Network lost during request"); }
        }
    }
    if (browser_state==1 && (uint32_t)(now-g_browser_started_ms)>=g_browser_timeout_ms) {
        if (browser_pcb) { tcp_abort(browser_pcb); browser_pcb=0; }
        browser_state=-1;
        browser_set_message("Network request timed out");
        serial_write("[net] Browser request timed out and was aborted\n");
    }
    if (!g_dns_probe_started && nexora_lwip_link_ready()) {
        ip_addr_t result; err_t e;
        g_dns_probe_started=1;
        serial_write("[net] DHCP lease active; testing DNS for example.com\n");
        e=dns_gethostbyname("example.com",&result,dns_probe_done,0);
        if (e==ERR_OK) dns_probe_done("example.com",&result,0);
        else if (e!=ERR_INPROGRESS) serial_write("[net] DNS probe could not start\n");
    }
}
int nexora_lwip_link_ready(void) { return g_initialized && netif_is_up(&g_netif) && !ip4_addr_isany_val(*netif_ip4_addr(&g_netif)); }
uint32_t nexora_lwip_ip_address(void) { return g_initialized ? ip4_addr_get_u32(netif_ip4_addr(&g_netif)) : 0; }


/* Text browser transport to a host-side HTTPS bridge. The bridge performs
 * verified HTTPS using the host TLS stack; guest-to-bridge traffic stays HTTP. */
#define BROWSER_QUERY_MAX 96
#define BROWSER_RESPONSE_MAX 1536
static char browser_query[BROWSER_QUERY_MAX];
static char browser_message[96] = "Ready - enter a Google search";
static char browser_response[BROWSER_RESPONSE_MAX];
/* browser_state and browser_pcb are declared with the network globals above. */
static char browser_request[512];
static size_t browser_request_len;
static void browser_set_message(const char *s) {
    size_t i=0; while(s && s[i] && i<sizeof(browser_message)-1){browser_message[i]=s[i];i++;} browser_message[i]=0;
}
static void browser_set_response(const char *s, size_t n) {
    if(n>=sizeof(browser_response)) n=sizeof(browser_response)-1;
    for(size_t i=0;i<n;i++) browser_response[i]=(s[i]>=32 || s[i]=='\n' || s[i]=='\r' || s[i]=='\t')?s[i]:' ';
    browser_response[n]=0;
}
static void browser_error(void *arg, err_t err) {
    (void)arg; (void)err; browser_pcb=0; browser_state=-1;
    browser_set_message("TCP connection failed");
}
static err_t browser_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    (void)arg; (void)err;
    if(!p){ browser_pcb=0; if(tcp_close(pcb)!=ERR_OK) tcp_abort(pcb); return ERR_OK; }
    char raw[BROWSER_RESPONSE_MAX]; size_t n=p->tot_len;
    if(n>sizeof(raw)-1)n=sizeof(raw)-1;
    pbuf_copy_partial(p,raw,(u16_t)n,0); raw[n]=0;
    char *body=raw; for(size_t i=0;i+3<n;i++) if(raw[i]=='\r'&&raw[i+1]=='\n'&&raw[i+2]=='\r'&&raw[i+3]=='\n'){body=&raw[i+4];break;}
    if(n>=12 && raw[0]=='H'&&raw[1]=='T'&&raw[2]=='T'&&raw[3]=='P') {
        if(raw[9]=='2' && raw[10]=='0' && raw[11]=='0') {
            browser_set_message("Google search results received");
            browser_set_response(body,n-(size_t)(body-raw));
        } else {
            browser_set_message("Search bridge returned an error");
            browser_set_response(body,n-(size_t)(body-raw));
        }
    } else { browser_set_message("Invalid response from search bridge"); browser_set_response(raw,n); }
    browser_state=2; tcp_recved(pcb,p->tot_len); pbuf_free(p); tcp_recv(pcb,0);
    if(tcp_close(pcb)!=ERR_OK)tcp_abort(pcb); browser_pcb=0; return ERR_OK;
}
static err_t browser_connected(void *arg, struct tcp_pcb *pcb, err_t err) {
    (void)arg; if(browser_state!=1) { tcp_abort(pcb); browser_pcb=0; return ERR_ABRT; } if(err!=ERR_OK){browser_state=-1;browser_set_message("Search bridge connection failed");return err;}
    tcp_recv(pcb,browser_recv);
    err_t e=tcp_write(pcb,browser_request,(u16_t)browser_request_len,TCP_WRITE_FLAG_COPY);
    if(e==ERR_OK){tcp_output(pcb);browser_set_message("Search request sent to local HTTPS bridge");}
    else {browser_state=-1;browser_set_message("Could not send HTTP request");}
    return e;
}
static void browser_dns_done(const char *name,const ip_addr_t *addr,void *arg) {
    (void)name;(void)arg;
    if (browser_state!=1) return; /* Ignore late DNS callbacks after timeout/cancel. */
    if(!addr||ip_addr_isany(addr)){browser_state=-1;browser_set_message("DNS lookup failed");return;}
    browser_pcb=tcp_new(); if(!browser_pcb){browser_state=-1;browser_set_message("Out of TCP memory");return;}
    tcp_err(browser_pcb,browser_error); err_t e=tcp_connect(browser_pcb,addr,8080,browser_connected);
    if(e!=ERR_OK){tcp_abort(browser_pcb);browser_pcb=0;browser_state=-1;browser_set_message("Could not start TCP connection");}
}
int nexora_browser_search(const char *query) {
    if(!query||!query[0]){browser_set_message("Type a search query first");return 0;}
    if(!nexora_lwip_link_ready()){browser_set_message("Network unavailable - waiting for DHCP");browser_state=-1;return 0;}
    if(browser_pcb){tcp_abort(browser_pcb);browser_pcb=0;}
    size_t q=0;while(query[q]&&q<sizeof(browser_query)-1){browser_query[q]=query[q];q++;}browser_query[q]=0;
    /* Percent-encode all non-unreserved bytes. */
    size_t w=0; const char *prefix="GET /search?q="; while(*prefix&&w<sizeof(browser_request)-1)browser_request[w++]=*prefix++;
    static const char hex[]="0123456789ABCDEF";
    for(size_t i=0;browser_query[i]&&w+4<sizeof(browser_request)-1;i++){
        unsigned char c=(unsigned char)browser_query[i];
        if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')browser_request[w++]=(char)c;
        else if(c==' '){browser_request[w++]='+';}
        else {browser_request[w++]='%';browser_request[w++]=hex[c>>4];browser_request[w++]=hex[c&15];}
    }
    const char *suffix=" HTTP/1.0\r\nHost: nexoraos-search-bridge\r\nUser-Agent: NexoraOS-TextBrowser/0.2\r\nConnection: close\r\n\r\n";
    while(*suffix&&w<sizeof(browser_request)-1)browser_request[w++]=*suffix++;
    browser_request[w]=0;browser_request_len=w;browser_response[0]=0;browser_state=1;g_browser_started_ms=sys_now();browser_set_message("Connecting to host search bridge...");
    ip_addr_t result; IP4_ADDR(ip_2_ip4(&result),10,0,2,2);
    browser_dns_done("host search bridge",&result,0);
    return browser_state >= 0;
}
int nexora_browser_state(void){return browser_state;}
const char *nexora_browser_message(void){return browser_message;}
const char *nexora_browser_response(void){return browser_response;}

/* Deliberately fail closed until a vetted TLS provider is linked and tested. */
int nexora_native_tls_available(void) { return 0; }
