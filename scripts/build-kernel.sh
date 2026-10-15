#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
for tool in clang ld.lld; do
  command -v "$tool" >/dev/null 2>&1 || { echo "Missing required tool: $tool" >&2; exit 2; }
done
mkdir -p build
if command -v nasm >/dev/null 2>&1; then
  nasm -f elf64 src/entry.asm -o build/entry.o
elif [ ! -f build/entry.o ]; then
  echo "NASM is missing and build/entry.o does not exist; install NASM." >&2
  exit 2
else
  echo "NASM unavailable: using the bundled entry.o (entry.asm unchanged in this release)." >&2
fi
CFLAGS=(--target=x86_64-unknown-none-elf -std=c11 -O2 -Wall -Wextra -Werror -Wno-misleading-indentation
  -ffreestanding -fno-stack-protector -fno-pic -fno-pie -mno-red-zone
  -mcmodel=kernel -mgeneral-regs-only -I src -I third_party/limine)
for source in src/kernel.c src/serial.c src/framebuffer.c src/memory.c src/keyboard.c src/mouse.c src/vfs.c src/pkg.c src/blockdev.c src/fat32.c src/emrefs.c; do
  object="build/$(basename "${source%.c}").o"
  echo "CC $source"
  clang "${CFLAGS[@]}" -c "$source" -o "$object"
done
ld.lld -T linker.ld -nostdlib -o build/kernel.elf \
  build/kernel.o build/serial.o build/framebuffer.o build/memory.o \
  build/keyboard.o build/mouse.o build/vfs.o build/pkg.o \
  build/blockdev.o build/fat32.o build/emrefs.o build/entry.o
echo "Kernel built: build/kernel.elf (ISO creation is a separate step)."
