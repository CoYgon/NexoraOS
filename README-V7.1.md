# NexoraOS Classic V7.1 — Security groundwork + System Monitor

This release builds on the supplied V7.0 source archive. It adds a system-monitor application and exposes current memory/network counters. It also makes the native TLS status explicit and fail-closed. **It does not claim to complete in-kernel TLS.**

## Added

- **System Monitor** app card and Start menu entry. It displays reported usable physical RAM, heap used/capacity, tracked allocated physical pages, DHCP/IP readiness, VirtIO RX/TX packet counts, RX drops, TX errors, and RDRAND availability. It refreshes periodically while open.
- **VirtIO network counters** for received packets, transmitted packets, dropped RX frames and TX errors. These are driver counters, not a substitute for a complete lwIP protocol-statistics dashboard.
- **Memory allocator telemetry** for currently tracked allocated pages.
- **Explicit native TLS availability API** (`nexora_native_tls_available()` currently returns false). Browser UI reports native TLS as not integrated rather than suggesting that the kernel verifies HTTPS.
- **RNG status display** clarifies that RDRAND availability is not the same as a seeded cryptographic DRBG.

## Security status

The V7.0 archive does not include mbedTLS sources, a kernel-compatible cryptographic backend, a vetted DRBG implementation, or a CA trust bundle. This environment could not download dependencies, so no third-party TLS code was fabricated or silently substituted. Native TLS, X.509 chain validation, hostname/SAN validation, validity-time checking and HTTPS directly over lwIP remain **not implemented**. The current browser search still depends on the V6.9 host-side HTTPS bridge. Do not use plaintext HTTP as a fallback for sensitive data.

To finish native TLS, vendor a pinned, licensed mbedTLS release, configure its freestanding platform hooks, use a vetted DRBG seeded from a trustworthy entropy source, provide a maintained CA bundle and valid time source, and connect TLS records to lwIP raw TCP callbacks. Then test valid certificates and negative cases (untrusted CA, hostname mismatch, expired certificate, timeout, disconnect, and packet loss) in QEMU.

## Build / test

```sh
make clean kernel-c-check
./tests/run_all_tests.sh
```

In this environment, `make clean kernel-c-check` succeeded and all existing host test groups passed, including three search-bridge parser tests and ten network-hardening source checks. A real bootable ISO and live QEMU networking were **not** tested because NASM, xorriso and QEMU were unavailable. The monitor UI has compile/link coverage but has not been visually validated in a running VM.
