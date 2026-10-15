#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
CC_BIN="${CC:-cc}"
OUT="/tmp/emreos-vfs-test-$$"
trap 'rm -f "$OUT"' EXIT HUP INT TERM
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -Isrc tests/vfs_test.c src/vfs.c -o "$OUT"
"$OUT"
