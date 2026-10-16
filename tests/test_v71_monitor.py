#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
kernel = (root / "src/kernel.c").read_text()
virtio = (root / "src/net/virtio_net.c").read_text()
header = (root / "src/net/lwip_port.h").read_text()
port = (root / "src/net/lwip_port.c").read_text()
checks = {
    "monitor app included in desktop app list": "\"MONITOR\"" in kernel and "#define APP_COUNT 7" in kernel,
    "heap telemetry rendered": "heap_used()" in kernel and "heap_capacity()" in kernel,
    "physical page telemetry exposed": "memory_allocated_pages()" in kernel,
    "RX/TX packet and error counters exposed": all(x in virtio for x in ("g_rx_packets++", "g_tx_packets++", "g_rx_drops++", "g_tx_errors++")),
    "network counters rendered": all(x in kernel for x in ("virtio_net_rx_packets()", "virtio_net_tx_packets()", "virtio_net_rx_drops()", "virtio_net_tx_errors()")),
    "native TLS status API explicit": "nexora_native_tls_available" in header and "int nexora_native_tls_available(void) { return 0; }" in port,
    "RNG status distinguishes hardware RNG from DRBG": "RDRAND: AVAILABLE (NOT A DRBG)" in kernel,
    "native TLS not misrepresented in UI": "NATIVE TLS: NOT YET INTEGRATED" in kernel,
}
for name, ok in checks.items():
    print(("PASS" if ok else "FAIL") + ": " + name)
if not all(checks.values()):
    raise SystemExit(1)
print(f"V7.1 monitor/security status checks passed ({len(checks)} checks).")
