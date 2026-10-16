# NexoraOS Classic V6.4 — lwIP + VirtIO networking

V6.4 adds a freestanding lwIP IPv4 networking path and a polling VirtIO legacy PCI Ethernet driver. It preserves the V6.3 desktop, heap, asset APIs, VFS, package manager, and storage code.

## Network path

- `src/net/virtio_net.c`: PCI configuration-space scan for legacy VirtIO-net (`1AF4:1000`), I/O BAR setup, legacy feature negotiation, 256-entry RX/TX split virtqueues, DMA address translation from Limine's kernel-address response, packet polling and transmission.
- `src/net/lwip_port.c`: `NO_SYS` lwIP initialization, `netif` glue, `sys_now()` TSC timer with PIT channel-2 calibration, DHCP startup, timer polling, hostname and a DNS lookup for `example.com`, followed by a plain-HTTP TCP connectivity probe to port 80 after a lease is acquired.
- `src/ports/lwip/lwipopts.h` and `src/ports/lwip/arch/cc.h`: freestanding compiler/configuration glue.
- The Makefile now compiles and links the lwIP core, IPv4, DHCP, DNS, ARP, ICMP, TCP/UDP and Ethernet modules into the kernel.
- `scripts/run.sh` configures QEMU user-mode NAT and a legacy VirtIO NIC. The NIC is polled because this kernel does not yet have a general interrupt subsystem or scheduler.

## Build and run

Install `clang`, `ld.lld`, `nasm`, `make`, `xorriso`, and the QEMU x86-64 system emulator. Then run:

```sh
./scripts/build.sh
./scripts/run.sh
```

The QEMU run script adds `-netdev user,id=net0 -device virtio-net-pci,disable-modern=on,netdev=net0`. QEMU user networking normally provides DHCP, DNS forwarding, and outbound NAT; this does not expose the guest directly to the LAN.

Serial output should report the detected VirtIO MAC, lwIP initialization, a DHCP-assigned IPv4 address, a successful DNS lookup, and (if outbound HTTP is permitted) a TCP/HTTP response from `example.com`. If the NIC is not detected, ensure the emulator command keeps `disable-modern=on`; this driver intentionally targets the legacy I/O-BAR interface and a 256-entry queue.

## Validation and limits

Run:

```sh
./tests/run_all_tests.sh
make kernel-c-check
```

`kernel-c-check` compiles every kernel and lwIP C translation unit and links them into a C-only validation ELF using `kmain` as the entry. It does **not** replace the actual NASM entry object and is not a bootable ISO.

The current development environment does not have NASM, xorriso, or QEMU, so I could not boot this exact build or verify a live DHCP lease/DNS response here. The C sources compile and the C-only ELF links, and the existing host regression suite passes. Treat live networking as implemented but awaiting a real QEMU boot test, not as proven end-to-end yet.

The driver currently supports only QEMU's legacy VirtIO-net PCI mode with a 256-entry queue, IPv4, and polling. It does not yet provide IPv6, interrupt-driven RX/TX, sockets/netconn APIs, TLS, a browser, or a general user-space network API. lwIP's raw TCP/UDP APIs are available to future kernel services. Do not test on physical hardware; use QEMU.
