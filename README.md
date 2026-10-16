# NexoraOS Classic V7.0

V7.0 focuses on network failure handling and safer entropy plumbing. Read [README-V7.0.md](README-V7.0.md) for exact scope, test results, and the still-open native TLS work.

## Build and test

```sh
make clean kernel-c-check
./tests/run_all_tests.sh
```

A bootable ISO and real QEMU/network validation additionally require NASM, xorriso, and QEMU. This archive does not claim that kernel-native HTTPS is complete; Google search still uses the verified host-side bridge until a vetted TLS engine and CA verification path are integrated.


## V7.1 security groundwork and system monitor

See `README-V7.1.md` for this revision's implementation and test status. Native in-kernel TLS remains explicitly unavailable; do not treat the host HTTPS bridge as kernel TLS.
