#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

# Check prerequisites before cleaning the existing build directory.
missing=()
for tool in clang ld.lld nasm make xorriso; do
  command -v "$tool" >/dev/null 2>&1 || missing+=("$tool")
done
if ((${#missing[@]})); then
  printf 'EmreOS build prerequisites missing: %s\n' "${missing[*]}" >&2
  printf 'Install the missing tools, then run ./scripts/build.sh again. Existing build files were not cleaned.\n' >&2
  exit 2
fi

LIMINE_BRANCH="${LIMINE_BRANCH:-v8.x-binary}"
mkdir -p third_party build

if [ ! -f third_party/limine/Makefile ]; then
  command -v git >/dev/null 2>&1 || { echo "git is required to fetch Limine." >&2; exit 2; }
  git clone --depth=1 --branch "$LIMINE_BRANCH" https://github.com/limine-bootloader/limine.git third_party/limine
fi

make clean
make
make -C third_party/limine

rm -rf build/iso_root
mkdir -p build/iso_root/boot/limine
cp build/kernel.elf build/iso_root/boot/kernel.elf
cp limine.conf build/iso_root/limine.conf

for f in limine-bios.sys limine-bios-cd.bin limine-uefi-cd.bin; do
  if [ -f "third_party/limine/$f" ]; then
    cp "third_party/limine/$f" build/iso_root/boot/limine/
  fi
done

xorriso -as mkisofs \
  -b boot/limine/limine-bios-cd.bin \
  -no-emul-boot -boot-load-size 4 -boot-info-table \
  --efi-boot boot/limine/limine-uefi-cd.bin \
  -efi-boot-part --efi-boot-image --protective-msdos-label \
  build/iso_root -o build/emreos.iso

third_party/limine/limine bios-install build/emreos.iso
echo "Built: build/emreos.iso"
