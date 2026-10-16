#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
./tests/run_vfs_tests.sh
./tests/run_pkg_tests.sh
./tests/run_storage_tests.sh
./tests/run_heap_tests.sh
./tests/run_assets_tests.sh
./tests/run_framebuffer_tests.sh

python3 "$(dirname "$0")/test_search_bridge.py"

python3 "$(dirname "$0")/test_tls_policy.py"

python3 "$(dirname "$0")/test_network_hardening.py"
python3 "$(dirname "$0")/test_v71_monitor.py"
