#!/usr/bin/env python3
"""Static regression checks for the freestanding network safety paths."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
net = (root / "src/net/lwip_port.c").read_text()
virtio = (root / "src/net/virtio_net.c").read_text()
opts = (root / "src/ports/lwip/lwipopts.h").read_text()

checks = {
    "RDRAND capability checked before use": "hardware_rng_available()" in net and "1u<<30" in net,
    "RNG has bounded retries": "retry<10" in net,
    "RNG fails closed without PRNG fallback": "No timestamp/XOR or PRNG fallback" in net,
    "Browser request timeout exists": "g_browser_timeout_ms = 15000u" in net and "Network request timed out" in net,
    "Late DNS callback ignored after cancellation": "if (browser_state!=1) return" in net,
    "DHCP/IP loss aborts active request": "Network lost during request" in net,
    "RX descriptor IDs validated": "id>=g_rx_size" in virtio,
    "Malformed RX descriptors recycled": "Recycle the descriptor even when the packet was malformed" in virtio,
    "DMA queue physical alignment checked": "page_phys&4095u" in virtio,
    "lwIP raw/no-OS mode explicit": "#define NO_SYS                         1" in opts,
}
failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(("PASS" if ok else "FAIL") + ": " + name)
if failed:
    raise SystemExit(1)
print(f"Network hardening source checks passed ({len(checks)} checks).")
