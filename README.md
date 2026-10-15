# EmreOS — x86_64 kernel starter

EmreOS is a small educational operating-system kernel written in freestanding C, with a tiny x86_64 Assembly entry point. It boots through the Limine boot protocol and is intended to run in QEMU first.

## What this starter includes

- x86_64 freestanding kernel entry
- Limine boot protocol requests for framebuffer and memory map
- serial logging to COM1 (visible with QEMU `-serial stdio`)
- framebuffer fill + simple pixel/rectangle drawing
- memory-map reporting and a deliberately small physical-page allocator
- polling PS/2 keyboard and mouse input (best-effort; depends on emulated controller)
- draggable desktop windows and a built-in, allow-listed kernel command shell
- basic physical-page allocation tracking with invalid-free/double-free rejection
- ISO build and QEMU launch scripts for Ubuntu under WSL2

This is a learning kernel, not a daily-use operating system. It does not yet have userspace, paging ownership, process scheduling, a filesystem, USB input, or a full desktop.

## Windows setup

1. Install WSL2 and Ubuntu from an elevated PowerShell:

   ```powershell
   wsl --install -d Ubuntu
   ```

   Reboot if Windows asks. Open Ubuntu from the Start menu.

2. In Ubuntu, install build tools:

   ```bash
   sudo apt update
   sudo apt install -y build-essential clang lld nasm xorriso mtools git make qemu-system-x86
   ```

3. Put this project somewhere accessible from WSL. If you extracted it to `C:\\Users\\YOUR_USER\\Downloads\\EmreOS`, access it in Ubuntu as `/mnt/c/Users/YOUR_USER/Downloads/EmreOS`. Then:

   ```bash
   cd /mnt/c/Users/YOUR_USER/Downloads/EmreOS
   chmod +x scripts/*.sh
   ```

4. Build Limine bootloader files and the ISO:

   ```bash
   ./scripts/build.sh
   ```

   The script clones Limine's stable branch into `third_party/limine`, builds its host utility, and creates `build/emreos.iso`. An internet connection is needed on the first build.

5. Run EmreOS:

   ```bash
   ./scripts/run.sh
   ```

   Serial logs are printed in the terminal. Close QEMU normally or press `Ctrl+C` in the terminal.

## Built-in terminal shell

On the desktop, open the **TERMINAL** card. Click inside the QEMU window and type commands. The shell runs inside the kernel and accepts only the built-in commands below; it does not pass input to the host OS.

- `help` — list commands
- `clear` — clear shell output
- `echo TEXT` — print text
- `uname` — report kernel/architecture
- `mem` — report usable memory bytes from Limine
- `pages` — report page size and allocator status
- `version` / `about` — show build information
- `exit` — close the terminal window

The PS/2 keyboard driver currently supports a limited lowercase ASCII set, digits, space, Enter and Backspace. Shift/Caps Lock, USB keyboards, pipes, redirection, command chaining, programs/processes and a POSIX-compatible shell are not implemented.

## Expected first boot

The QEMU window should show a dark background with a green EmreOS desktop if a framebuffer is provided. Serial output reports kernel startup and device status. Open the terminal card to use the built-in command shell.

## Build manually

```bash
make clean
make
```

The output kernel is `build/kernel.elf`. The ISO script places it with the Limine configuration and required boot files.

## Project map

- `src/entry.asm` — Assembly entry shim
- `src/kernel.c` — boot requests, startup, memory map, basic loop
- `src/serial.c`, `src/serial.h` — COM1 serial logging
- `src/framebuffer.c`, `src/framebuffer.h` — pixel/rectangle drawing
- `src/memory.c`, `src/memory.h` — basic physical-page allocator
- `src/keyboard.c`, `src/keyboard.h` — polling PS/2 scancode reader
- `limine.conf` — boot menu configuration
- `linker.ld` — kernel linker layout
- `scripts/build.sh` — Limine + ISO build
- `scripts/run.sh` — QEMU launch

## Notes and limitations

- Limine's protocol and boot assets can evolve. The script uses the `v8.x-binary` branch. If that branch is unavailable, check the current Limine release and update `LIMINE_BRANCH` in `scripts/build.sh`.
- The page allocator is still a minimal demonstration. It tracks up to 8192 outstanding allocations and rejects invalid/double frees, but it does not zero pages, provide virtual mappings, use locks, or implement a general-purpose heap. The fixed tracking/free-stack capacities are deliberate limits.
- Keyboard input uses legacy PS/2 ports and polling. It is not a USB HID driver.
- The framebuffer uses the pixel format reported by Limine and currently assumes 32 bits per pixel for drawing.
- This is not a hardened or daily-use OS. There is no userspace isolation, virtual-memory manager, interrupt-driven input, filesystem, privilege separation, or complete device-driver stack. The shell is a small built-in command interpreter, not a Unix shell. Do not run on real hardware; test in QEMU.
