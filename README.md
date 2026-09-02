# kernel-template

A small x86_64 kernel template for TurkOsdev. It boots with Limine, initializes a ring-0 environment, maps a tiny userspace image with user-accessible page-table entries, drops to ring 3 with `iretq`, and lets `/userland/hello.c` print through a minimal `int 0x80` syscall ABI.

## What is included

- Limine boot protocol (x86_64)
- higher-half kernel
- COM1 serial logger
- tiny physical page allocator from the Limine memory map
- user page mappings in the active x86_64 page tables
- GDT + 64-bit TSS with `rsp0`
- IDT entry `0x80` callable from ring 3
- real CPL3 transition with `iretq`
- `/userland/hello.c`
- tiny libc (`write`, `puts`, `strlen`, `exit`)
- QEMU target with deterministic serial output

Expected output:

```text
[boot] Limine handoff OK
[kernel] ring 0 initialized
[user] entering ring 3
Hello from userspace
[kernel] userspace exited
```

## Dependencies

On a typical Linux host:

```sh
# Arch / CachyOS
sudo pacman -S --needed base-devel git xorriso qemu-system-x86

# Debian / Ubuntu
sudo apt install build-essential git xorriso qemu-system-x86
```

Limine itself is fetched automatically from the current `v12.x` branch on the first image build.

## Build and run

```sh
make run
```

Build only the kernel and embedded userspace without downloading Limine:

```sh
make kernel
```

The bootable ISO is written to:

```text
dist/kernel-template.iso
```

## Layout

```text
kernel/
  arch/x86_64/   GDT, TSS, IDT, syscall entry, ring-3 transition
  mm/            tiny PMM/VMM helpers
  lib/           freestanding string helpers
  main.c         Limine handoff and kernel startup
  syscall.c      minimal syscall dispatcher
  user.c         userspace loader/mapping
userland/
  hello.c
  libc/          tiny userspace libc
  include/libc.h
scripts/
  fetch-limine.sh
```

## Syscall ABI

This is intentionally tiny and educational, not POSIX.

- `rax = 1`, `rdi = buffer`, `rsi = length` -> write to kernel serial console
- `rax = 60`, `rdi = status` -> terminate the demo userspace task

The interrupt gate at vector `0x80` has DPL 3. On entry from userspace, the CPU switches to the ring-0 stack configured in the TSS before the kernel dispatches the syscall.

## Notes

This repository is a starting point, not a complete OS. It intentionally omits scheduling, ELF loading at runtime, copy-from-user fault recovery, a general VM subsystem, SMP, and interrupt-controller setup. The userspace image is linked separately and embedded into the kernel image during the build so the privilege-boundary example stays easy to follow.
