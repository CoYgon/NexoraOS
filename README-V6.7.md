# NexoraOS Classic V6.7 — Nexora Web (experimental text browser)

V6.7 adds a small desktop browser window that accepts a search query, resolves `www.google.com` through lwIP DNS, opens a TCP connection to port 80, and sends a URL-encoded HTTP search request. The window reports network/DHCP state, request status, and a bounded text preview of the HTTP response or redirect target.

## Current limitations — read before use

- This is an **experimental HTTP text client**, not a general-purpose HTML browser.
- Google redirects HTTP search requests to HTTPS. V6.7 does **not** implement TLS, certificate validation, JavaScript, CSS, HTML layout, cookies, or HTTPS, so it cannot yet display Google search results. When Google redirects to HTTPS, the browser shows that status instead of pretending the search worked.
- The browser needs a working VirtIO network device, DHCP lease, DNS configuration, and QEMU networking. It is not evidence that internet access works until run and checked in QEMU.
- Keyboard input uses the existing PS/2 Set-1 polling driver; USB keyboard support is not included.

## Use

1. Build with `make clean kernel-c-check` to verify all kernel and lwIP C sources compile and link.
2. Build/run the full bootable image using the project's normal Limine/ISO workflow and configure a QEMU VirtIO network device with user-mode networking.
3. Open **Browser** from the desktop card or Start menu, type a query, and press Enter (or click Search).
4. Check the serial console and browser status. A Google HTTPS redirect is expected until TLS is added.

## Next engineering milestone

Integrate a small, audited TLS 1.2/1.3 implementation with a trusted CA bundle and working entropy source, then implement bounded HTML text extraction. A real visual web engine is a much larger project and is intentionally out of scope for this text-browser milestone.
