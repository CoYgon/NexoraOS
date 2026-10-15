# EmrePkg — EmreOS package manager

EmrePkg adds an apt/pacman-inspired command interface to the EmreOS built-in shell. It uses a small built-in package catalog and tracks installed metadata, dependency relationships, and core-package protections.

## Commands

```text
pkg help
pkg search <term>
pkg list
pkg info <name>
pkg install <name>
pkg remove <name>
pkg update
pkg upgrade
pkg repos
pkg autoremove
```

Example session:

```text
pkg search calc
pkg info calc
pkg install calc
pkg list
pkg remove calc
```

## Included catalog

- `base-utils` — core package (protected)
- `pkg` — EmrePkg itself (protected)
- `textedit` — text editor package metadata
- `calc` — calculator package metadata
- `sysmon` — system monitor package metadata
- `filetools` — file management package metadata
- `devkit` — development tools package metadata
- `docs` — manuals package metadata
- `netdiag` — network diagnostics package metadata

## Current limitations

This is a first-stage package manager, not yet a full binary package installer. The catalog is compiled into the kernel, package state is volatile RAM state, and `install`/`remove` update package metadata only. No network repository, package archive format, cryptographic signature verification, persistent database, executable loader, or dependency version solver exists yet. `pkg update` explicitly performs no network request; it confirms the local built-in index is present. These limitations are surfaced in command output so the interface does not claim capabilities that the OS does not yet have.

## Tests

Run `./tests/run_pkg_tests.sh` to exercise search, install, list, core-package protection, remove, update, and upgrade behavior.
