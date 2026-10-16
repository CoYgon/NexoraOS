#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

command -v qemu-system-x86_64 >/dev/null 2>&1 || {
  echo "qemu-system-x86_64 is required to run NexoraOS." >&2
  exit 2
}
if [ ! -f build/nexoraos.iso ]; then
  echo "ISO not found. Run ./scripts/build.sh first." >&2
  exit 1
fi

asset_disk_args=()
if [ -f build/nexoraos-assets.img ]; then
  # The current block driver supports legacy primary-channel ATA PIO; pc/i440fx exposes it.
  asset_disk_args=(-drive file=build/nexoraos-assets.img,format=raw,if=ide,index=0)
fi

echo "Nexora Web requires the host search bridge: python3 tools/nexora_search_bridge.py"
echo "The bridge must be running on TCP port 8080 before searching."

qemu-system-x86_64 \
  -M pc \
  -m 512M \
  -cpu qemu64,+rdrand \
  -vga std \
  -device i8042 \
  -device ps2-mouse \
  -netdev user,id=net0 \
  -device virtio-net-pci,disable-modern=on,netdev=net0 \
  "${asset_disk_args[@]}" \
  -cdrom build/nexoraos.iso \
  -boot d \
  -serial stdio \
  -no-reboot \
  -no-shutdown
