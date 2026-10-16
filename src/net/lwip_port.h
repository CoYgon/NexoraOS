#ifndef NEXORAOS_LWIP_PORT_H
#define NEXORAOS_LWIP_PORT_H
#include <stdint.h>
struct limine_kernel_address_response;
int nexora_secure_random(void *out, unsigned long len);
void nexora_lwip_set_kernel_address(volatile struct limine_kernel_address_response *response);
int nexora_lwip_init(void);
void nexora_lwip_poll(void);
int nexora_lwip_link_ready(void);
int nexora_secure_random_available(void);
int nexora_native_tls_available(void);
uint32_t nexora_lwip_ip_address(void);
int nexora_browser_search(const char *query);
int nexora_browser_state(void);
const char *nexora_browser_message(void);
const char *nexora_browser_response(void);
#endif
