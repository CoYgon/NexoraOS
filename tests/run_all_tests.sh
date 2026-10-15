#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
./tests/run_vfs_tests.sh
./tests/run_pkg_tests.sh
./tests/run_storage_tests.sh
