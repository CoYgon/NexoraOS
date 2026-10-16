#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
for tool in clang ld.lld nasm make; do
  command -v "$tool" >/dev/null 2>&1 || { echo "Missing required tool: $tool" >&2; exit 2; }
done
make clean all
echo "Kernel built: build/kernel.elf"
