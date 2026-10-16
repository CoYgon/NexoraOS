#ifndef NEXORAOS_LWIPOPTS_H
#define NEXORAOS_LWIPOPTS_H

/*
 * NexoraOS lwIP configuration.
 *
 * This is a freestanding kernel: no POSIX sockets, libc, threads, or
 * tcpip_thread are available yet. Use lwIP's NO_SYS raw API with the
 * polling legacy VirtIO PCI NIC; sockets/netconn remain disabled without a scheduler.
 */
#define NO_SYS                         1
#define SYS_LIGHTWEIGHT_PROT            0
#define LWIP_SOCKET                    0
#define LWIP_NETCONN                   0
#define LWIP_NETIF_API                 0
#define LWIP_TIMERS                    1
#define LWIP_CALLBACK_API              1
#define LWIP_RAW                       1
#define LWIP_UDP                       1
#define LWIP_TCP                       1
#define LWIP_DHCP                      1
#define LWIP_DNS                       1
#define LWIP_ICMP                      1
#define LWIP_IPV4                      1
#define LWIP_IPV6                      0
#define LWIP_ARP                       1
#define LWIP_ETHERNET                  1
#define LWIP_NETIF_LINK_CALLBACK       1
#define LWIP_NETIF_STATUS_CALLBACK     1
#define LWIP_NETIF_HOSTNAME             1
#define LWIP_DHCP_DOES_ACD_CHECK        1
#define LWIP_DHCP_AUTOIP_COOP           0
#define IP_REASSEMBLY                   0
#define IP_FRAG                         0
#define LWIP_NETIF_LOOPBACK             0
#define LWIP_STATS                     0
#define LWIP_DEBUG                     0
#define MEM_ALIGNMENT                  8
#define MEM_SIZE                       (32 * 1024)
#define MEMP_NUM_PBUF                  16
#define MEMP_NUM_UDP_PCB               4
#define MEMP_NUM_TCP_PCB               4
#define MEMP_NUM_TCP_PCB_LISTEN        2
#define MEMP_NUM_TCP_SEG               16
#define PBUF_POOL_SIZE                 16
#define PBUF_POOL_BUFSIZE               1600
#define TCP_MSS                        1460
#define TCP_WND                        (4 * TCP_MSS)
#define TCP_SND_BUF                    (4 * TCP_MSS)
#define LWIP_DHCP_MAX_NTP_SERVERS      0

/* Keep all heap allocations inside lwIP's configured pool. */
#define MEM_LIBC_MALLOC                0
#define MEMP_MEM_MALLOC                0

#endif
