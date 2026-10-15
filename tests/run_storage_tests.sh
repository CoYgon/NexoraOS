#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
OUT="/tmp/emreos_storage_test_$$"
trap 'rm -f "$OUT"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -Wno-misleading-indentation -DEMREOS_HOST_TEST -I src tests/storage_test.c src/blockdev.c src/fat32.c src/emrefs.c -o "$OUT"
"$OUT"
