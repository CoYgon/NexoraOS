# NexoraOS Classic V7.0 — network hardening milestone

This revision improves the existing freestanding lwIP/VirtIO network path. It is **not** a completed native-HTTPS release: the V6.9 archive does not contain the mbedTLS source tree, an X.509 verifier build, or a CA bundle suitable for this freestanding kernel. The native TLS contract remains disabled; the browser still uses the V6.9 host-side HTTPS bridge. No custom/incomplete TLS implementation is presented as secure.

## Changes in V7.0

- Replaced the xorshift/TSC `LWIP_RAND` source with a hardware RDRAND-backed function. CPUID capability is checked, retries are bounded, and failures return failure/zero rather than silently falling back to predictable timestamps or a software PRNG. This is an entropy source hook, **not** a TLS DRBG by itself.
- Added browser network-request timeout handling, cancellation on DHCP/IP loss, and guards against late DNS/TCP callbacks after cancellation.
- Hardened legacy VirtIO RX descriptor handling: invalid descriptor IDs disable the NIC, malformed frames are dropped, and valid descriptors are recycled even if packet allocation/validation fails.
- Added DMA queue physical-address/alignment and RX buffer address sanity checks.
- Added source-level regression checks for the above failure paths.

## Build and tests

```sh
make clean kernel-c-check
./tests/run_all_tests.sh
```

`kernel-c-check` compiles and links the C/LwIP objects only. It does not assemble the boot entry, produce an ISO, or prove QEMU networking. QEMU and NASM were not available in the build environment, so live DHCP/DNS/TCP/UDP, packet-loss, and DMA behavior are not certified by this release.

## Required work before native HTTPS can be called complete

1. Vendor a pinned mbedTLS release and its license/source files into the project; do not link a host shared library into the freestanding kernel.
2. Configure only supported TLS 1.2/1.3 features for this target and provide freestanding platform hooks (allocator, time, entropy).
3. Implement a TLS DRBG seeded from a trustworthy source, certificate-chain validation, hostname/SAN verification, validity-time handling, and a CA bundle with an update path.
4. Connect the TLS engine to lwIP's raw TCP callbacks with bounded buffering, handshake/read/write timeouts, cancellation, and cleanup on TCP errors.
5. Add QEMU integration tests for DHCP, DNS over UDP, TCP connect/close, verified HTTPS, invalid CA, hostname mismatch, expired certificate, disconnect, timeout, and packet loss.

Do not remove the host bridge or claim direct kernel HTTPS until these tests pass.
