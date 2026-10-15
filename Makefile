BUILD := build
CC := clang
LD := ld.lld
NASM := nasm

CFLAGS := --target=x86_64-unknown-none-elf -std=c11 -O2 -Wall -Wextra \
          -ffreestanding -fno-stack-protector -fno-pic -fno-pie \
          -mno-red-zone -mcmodel=kernel -mgeneral-regs-only \
          -I src -I third_party/limine
LDFLAGS := -T linker.ld -nostdlib

C_SOURCES := src/kernel.c src/serial.c src/framebuffer.c src/memory.c src/keyboard.c src/mouse.c src/vfs.c src/pkg.c src/blockdev.c src/fat32.c src/emrefs.c
OBJECTS := $(patsubst src/%.c,$(BUILD)/%.o,$(C_SOURCES)) $(BUILD)/entry.o

.PHONY: all clean
all: $(BUILD)/kernel.elf

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/entry.o: src/entry.asm | $(BUILD)
	$(NASM) -f elf64 $< -o $@

$(BUILD)/kernel.elf: $(OBJECTS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

clean:
	rm -rf $(BUILD)
