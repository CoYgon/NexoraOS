BUILD := build
CC := clang
LD := ld.lld
NASM := nasm

CFLAGS := --target=x86_64-unknown-none-elf -std=c11 -O2 -Wall -Wextra \
          -ffreestanding -fno-stack-protector -fno-pic -fno-pie \
          -mno-red-zone -mcmodel=kernel -mgeneral-regs-only \
          -I src/freestanding -I src -I src/ports/lwip -I lwip/src/include -I third_party/limine
LDFLAGS := -T linker.ld -nostdlib
ASSET_CFLAGS := $(filter-out -mgeneral-regs-only,$(CFLAGS)) -fno-builtin

LWIP_SOURCES := \
  lwip/src/core/init.c lwip/src/core/def.c lwip/src/core/dns.c \
  lwip/src/core/inet_chksum.c lwip/src/core/ip.c lwip/src/core/mem.c \
  lwip/src/core/memp.c lwip/src/core/netif.c lwip/src/core/pbuf.c \
  lwip/src/core/raw.c lwip/src/core/stats.c lwip/src/core/sys.c \
  lwip/src/core/altcp.c lwip/src/core/altcp_alloc.c lwip/src/core/altcp_tcp.c \
  lwip/src/core/tcp.c lwip/src/core/tcp_in.c lwip/src/core/tcp_out.c \
  lwip/src/core/timeouts.c lwip/src/core/udp.c \
  lwip/src/core/ipv4/acd.c lwip/src/core/ipv4/dhcp.c \
  lwip/src/core/ipv4/etharp.c lwip/src/core/ipv4/icmp.c \
  lwip/src/core/ipv4/ip4.c lwip/src/core/ipv4/ip4_addr.c \
  lwip/src/netif/ethernet.c
SRC_SOURCES := src/kernel.c src/serial.c src/framebuffer.c src/memory.c src/heap.c src/assets.c src/keyboard.c src/mouse.c src/vfs.c src/pkg.c src/blockdev.c src/fat32.c src/emrefs.c src/net/virtio_net.c src/net/lwip_port.c
C_OBJECTS := $(patsubst src/%.c,$(BUILD)/%.o,$(SRC_SOURCES)) $(patsubst lwip/src/%.c,$(BUILD)/lwip_%.o,$(LWIP_SOURCES))
OBJECTS := $(C_OBJECTS) $(BUILD)/entry.o

.PHONY: all clean kernel-c-check
all: $(BUILD)/kernel.elf

kernel-c-check: $(C_OBJECTS)
	$(LD) $(LDFLAGS) -e kmain -o $(BUILD)/kernel-c-link-check.elf $(C_OBJECTS)
	@echo "C/LwIP objects compiled and linked (entry assembly/ISO not tested)."

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/lwip_%.o: lwip/src/%.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fno-builtin -c $< -o $@

# stb_truetype uses floating-point math. Keep the rest of the kernel GPR-only,
# but compile this isolated translation unit with x86-64 SSE2 enabled.
$(BUILD)/assets.o: src/assets.c | $(BUILD)
	$(CC) $(ASSET_CFLAGS) -c $< -o $@

$(BUILD)/entry.o: src/entry.asm | $(BUILD)
	$(NASM) -f elf64 $< -o $@

$(BUILD)/kernel.elf: $(OBJECTS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

clean:
	rm -rf $(BUILD)
