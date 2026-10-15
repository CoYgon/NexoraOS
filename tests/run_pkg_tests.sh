#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cc -std=c11 -Wall -Wextra -Werror -I src src/pkg.c tests/pkg_test.c -o /tmp/emreos_pkg_test
/tmp/emreos_pkg_test
rm -f /tmp/emreos_pkg_test
