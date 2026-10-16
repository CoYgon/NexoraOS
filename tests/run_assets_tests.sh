#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cc -std=c11 -O2 -Wall -Wextra -fno-builtin -iquote src tests/assets_test.c src/heap.c src/assets.c -o /tmp/nexoraos-assets-test
/tmp/nexoraos-assets-test
