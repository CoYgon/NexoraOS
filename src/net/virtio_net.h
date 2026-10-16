#ifndef NEXORAOS_VIRTIO_NET_H
#define NEXORAOS_VIRTIO_NET_H
#include <stdint.h>
#include "lwip/err.h"
struct limine_kernel_address_response;
struct netif;
void virtio_net_set_kernel_address(volatile struct limine_kernel_address_response *response);
int virtio_net_init(struct netif *netif);
int virtio_net_poll(void);
int virtio_net_is_ready(void);
uint64_t virtio_net_rx_packets(void);
uint64_t virtio_net_tx_packets(void);
uint64_t virtio_net_rx_drops(void);
uint64_t virtio_net_tx_errors(void);
const uint8_t *virtio_net_mac(void);
struct pbuf;
struct netif;
err_t virtio_net_linkoutput(struct netif *netif, struct pbuf *p);
#endif
