#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

for tool in dd mkfs.vfat mmd mcopy; do
  command -v "$tool" >/dev/null 2>&1 || {
    echo "Missing tool: $tool" >&2
    echo "Install dosfstools and mtools (e.g. sudo apt install dosfstools mtools)." >&2
    exit 2
  }
done

mkdir -p build
image=build/nexoraos-assets.img
# 128 MiB with 512-byte clusters ensures enough clusters for FAT32 validation.
dd if=/dev/zero of="$image" bs=1M count=128 status=progress
mkfs.vfat -F 32 -s 1 "$image"
mmd -i "$image" ::/NEXORA
mcopy -i "$image" assets/WALLPAPER.PNG ::/NEXORA/WALLPAP.PNG
mcopy -i "$image" assets/DejaVuSansMono.ttf ::/NEXORA/FONT.TTF
printf 'Created %s with /NEXORA/WALLPAP.PNG and /NEXORA/FONT.TTF\n' "$image"
printf 'Run ./scripts/run.sh to attach it automatically. The current disk driver is read-only.\n'
