# NexoraOS Classic V6.8 — working Google text search

V6.8 upgrades Nexora Web from a direct HTTP experiment to a small text-only Google search client. The guest still uses the in-kernel lwIP TCP/IP stack and VirtIO-net device. Since the freestanding kernel does not yet contain a TLS client or certificate store, a deliberately narrow host-side bridge performs Google HTTPS using Python's standard TLS certificate validation and returns plain-text search results to the guest.

## Start the search bridge

On the Linux/WSL host that will run QEMU, install Python 3 (no third-party Python packages are needed), then start:

```bash
python3 tools/nexora_search_bridge.py
```

Keep this terminal open. The bridge listens on TCP port 8080 and accepts loopback and QEMU user-network clients (`10.0.2.0/24`). It exposes only `/search`; it is not an arbitrary URL proxy. It validates the query length, uses HTTPS with certificate verification to contact Google, and returns a bounded plain-text list of result titles and URLs.

In another terminal, build and start NexoraOS:

```bash
./scripts/build.sh
./scripts/run.sh
```

The QEMU network device uses user-mode networking. From the NexoraOS desktop, open **Browser**, type a query using the existing PS/2 keyboard, and press Enter. The browser connects to `10.0.2.2:8080`, the QEMU host gateway, and displays the response returned by the bridge. Check the host bridge terminal for request logs.

## What is real, and what is not

- **Real Google search:** the host helper makes a live HTTPS request to Google and validates the TLS certificate using the host Python/OpenSSL trust store. Network access and Google's willingness to serve the request are required.
- **Guest networking:** the request from NexoraOS to the bridge travels over the guest's lwIP TCP stack and VirtIO-net driver.
- **Not a standalone native HTTPS browser:** TLS is performed by the host helper, not inside the kernel. The OS currently displays a compact text result list; it does not render arbitrary web pages, JavaScript, CSS, cookies, or HTTPS websites itself.
- **No false success:** if the bridge is not running, Google blocks the request, the host has no internet, or TLS validation fails, the browser shows an error response rather than fabricated results.

## Troubleshooting

- `TCP connection failed`: start the bridge first and confirm host TCP port 8080 is not blocked.
- `Network unavailable`: wait for DHCP and verify QEMU networking is enabled.
- `Google requires a CAPTCHA or consent page`: Google declined automated access; try again later or use a search provider/API with permitted access.
- `Could not reach Google over verified HTTPS`: check the host's internet connection, DNS, and system CA certificates.

## Tests

```bash
make clean kernel-c-check
./tests/run_all_tests.sh
python3 -m py_compile tools/nexora_search_bridge.py
```

The host-side tests exercise parsing with fixtures and do not require live internet. A live Google query must be tested on a machine with network access.
