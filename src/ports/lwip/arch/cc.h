#ifndef NEXORAOS_LWIP_ARCH_CC_H
#define NEXORAOS_LWIP_ARCH_CC_H

#include <stdint.h>
#include <stddef.h>

/* lwIP's compiler/architecture adaptation for x86-64 NexoraOS. */
typedef uint8_t  u8_t;
typedef int8_t   s8_t;
typedef uint16_t u16_t;
typedef int16_t  s16_t;
typedef uint32_t u32_t;
typedef int32_t  s32_t;
typedef uintptr_t mem_ptr_t;

#define BYTE_ORDER LITTLE_ENDIAN
#define LWIP_NO_INTTYPES_H 1
#define LWIP_NO_LIMITS_H 1
#define LWIP_NO_CTYPE_H 1
#define X8_F "02x"
#define U16_F "u"
#define S16_F "d"
#define X16_F "x"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"
#define SZT_F "zu"
uint32_t nexora_lwip_rand(void);
#define LWIP_RAND() nexora_lwip_rand()
#define LWIP_PLATFORM_DIAG(x) do { } while (0)
#define LWIP_PLATFORM_ASSERT(x) do { (void)(x); for (;;) __asm__ volatile ("cli; hlt"); } while (0)

#endif
