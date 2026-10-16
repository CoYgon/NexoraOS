#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cc -std=c11 -Wall -Wextra -Werror -I src src/pkg.c tests/pkg_test.c -o /tmp/nexoraos_pkg_test
/tmp/nexoraos_pkg_test
rm -f /tmp/nexoraos_pkg_test
