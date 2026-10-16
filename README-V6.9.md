# NexoraOS Classic V7.0 — TLS security hardening and native-port contract

This release hardens the existing Google search bridge's TLS behavior and records a strict interface/security contract for a future in-kernel TLS provider. It deliberately does **not** claim that TLS has moved into the kernel: this build environment has no vendored TLS engine (such as mbedTLS or BearSSL), no X.509 verification implementation, and no validated CA root bundle suitable for the freestanding kernel. Implementing a home-grown TLS stack would be unsafe.

## What changed

- The host-side search bridge now creates an explicit `ssl.create_default_context()` TLS context, requires `CERT_REQUIRED`, enables hostname verification, and never retries with verification disabled.
- `src/net/tls_native_contract.h` specifies the native TLS security contract: TLS 1.2 minimum, certificate required, CA chain and hostname validation required, and no insecure fallback.
- Existing guest-to-bridge transport remains lwIP TCP over QEMU user networking. Google HTTPS is still handled by the host helper; native in-kernel TLS is not yet active.
- Tests now assert that the bridge uses verified TLS and refuses insecure TLS configuration patterns.

## Why native TLS is not falsely marked complete

A real in-kernel HTTPS client needs a maintained TLS implementation, cryptographic primitives, X.509 parsing and signature validation, hostname validation, a trustworthy CA root store, entropy, and a nonblocking integration with lwIP's raw TCP callbacks. lwIP itself is TCP/IP, not TLS. None of those cryptographic components should be approximated or bypassed. The next implementation step is to vendor a pinned, licensed TLS library and CA bundle, port its allocator/entropy/time hooks, and drive its handshake from lwIP's asynchronous TCP callbacks.

## Build and tests

```sh
make clean kernel-c-check
./tests/run_all_tests.sh
python3 -m py_compile tools/nexora_search_bridge.py
```

A live Google search still requires internet access on the QEMU host and may be blocked by CAPTCHA/consent protections. A successful compile or fixture test does not prove a live TLS handshake.
