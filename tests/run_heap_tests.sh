#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cc -std=c11 -O2 -Wall -Wextra -iquote src tests/heap_test.c src/heap.c -o /tmp/nexoraos-heap-test
/tmp/nexoraos-heap-test
