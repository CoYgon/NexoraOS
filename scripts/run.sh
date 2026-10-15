#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

command -v qemu-system-x86_64 >/dev/null 2>&1 || {
  echo "qemu-system-x86_64 is required to run EmreOS." >&2
  exit 2
}
if [ ! -f build/emreos.iso ]; then
  echo "ISO not found. Run ./scripts/build.sh first." >&2
  exit 1
fi

qemu-system-x86_64 \
  -M q35 \
  -m 512M \
  -cpu qemu64 \
  -vga std \
  -device i8042 \
  -device ps2-mouse \
  -cdrom build/emreos.iso \
  -boot d \
  -serial stdio \
  -no-reboot \
  -no-shutdown
