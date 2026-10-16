#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cc -std=c11 -O2 -Wall -Wextra -iquote src tests/framebuffer_test.c src/framebuffer.c -o /tmp/nexoraos-framebuffer-test
/tmp/nexoraos-framebuffer-test
