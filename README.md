# kernel-template

A small x86_64 Limine kernel template with a real ring-3 userspace and an upstream musl libc build.

The goal is not to pretend this is Linux. The template implements enough of the Linux x86_64 process/syscall ABI for a statically linked musl program to start, use stdio, allocate memory, and exit. The **full upstream musl build produces its normal `libc.a`, crt objects, and headers**; the kernel-side syscall surface is intentionally much smaller than Linux, so libc functions that need filesystems, networking, signals, processes, or real threading are not usable until those kernel facilities exist.

## What is included

- Limine bootloader, BIOS + UEFI ISO
- x86_64 long mode kernel at ring 0
- GDT + TSS + ring-3 transition
- Limine framebuffer text console, mirrored to COM1
- ELF64 `PT_LOAD` userspace loader
- Linux-style initial process stack (`argc`, `argv`, `envp`, auxv)
- x86_64 `SYSCALL` / `SYSRET` path
- FS-base setup for musl TLS through `arch_prctl`
- anonymous user `mmap`, `mprotect`, and `munmap`
- upstream musl 1.2.6, built statically and unmodified
- `/userland/hello.c` using real `<stdio.h>`, `<stdlib.h>`, and `<string.h>`
- a libc-free syscall smoke program for kernel-side testing
- automated host/ELF checks and QEMU serial tests

## Layout

```text
kernel/
  arch/x86_64/       GDT, IDT, ring3 entry, SYSCALL/SYSRET
  include/kernel/
  mm/                physical allocation + user mappings
  main.c
  syscall.c          small Linux x86_64 syscall compatibility layer
  user.c             ELF loader + Linux initial stack/auxv
userland/
  hello.c            statically linked against upstream musl
tests/
  smoke.c            no-libc ring3 syscall exerciser
  host.sh             compile/link/ELF checks
  qemu.sh             serial boot assertions
scripts/
  fetch-limine.sh
  fetch-musl.sh
  build-musl.sh
```

## Dependencies

On a typical Debian/Ubuntu host:

```sh
sudo apt install build-essential curl xorriso qemu-system-x86
```

`make` downloads a pinned musl 1.2.6 release tarball and verifies its SHA-256 before building it. Limine is fetched separately by the existing Limine helper.

## Build and run

```sh
make run
```

`make run` opens the QEMU framebuffer window. Normal console text also mirrors to COM1; for debugging:

```sh
make run-serial
```

Expected demo output:

```text
[boot] Limine framebuffer ready
[kernel] ring 0 initialized
[user] entering ring 3
Hello from userspace
[musl] malloc + string are alive
[musl] stdio + malloc + string OK
[kernel] userspace exited
```

The graphical `run` target intentionally leaves QEMU open after userspace exits. The automated QEMU test adds `isa-debug-exit`, allowing the guest to terminate the test VM.

## musl build

The default userland is `musl`:

```sh
make userland
```

The flow is:

```text
musl-1.2.6.tar.gz
       |
       +-- SHA-256 verification
       |
       +-- configure --disable-shared
       |
       +-- full upstream libc.a + crt objects + headers
       |
       +-- musl-gcc -static userland/hello.c
       |
       +-- ELF64 executable loaded by the kernel
```

The musl source is kept out of Git. To build fully offline, provide a local release tarball:

```sh
MUSL_TARBALL=/path/to/musl-1.2.6.tar.gz make userland
```

## Kernel ABI currently implemented

The syscall numbers follow Linux x86_64 so upstream musl does not need a custom syscall patch.

| Syscall | Purpose in this template |
| --- | --- |
| `read` | stdin EOF stub |
| `write` | stdout/stderr console output |
| `close` | stdio descriptor stub |
| `mmap` | anonymous userspace mappings |
| `mprotect` | page write protection |
| `munmap` | removes user mappings |
| `brk` | deliberately unavailable so allocation falls back to mmap |
| `ioctl(TIOCGWINSZ)` | stdio terminal detection |
| `writev` | musl stdio flushing |
| `madvise` | no-op success for allocator cleanup |
| `getpid` | single-process PID 1 |
| `arch_prctl` | `ARCH_SET_FS` / `ARCH_GET_FS` for TLS |
| `set_tid_address` | single-thread bootstrap stub |
| `clock_gettime` | deterministic monotonic stub |
| `exit` / `exit_group` | terminates userspace |
| `getrandom` | deterministic bootstrap bytes; not cryptographic |

Everything else returns `-ENOSYS`. In particular, this is **not** yet a POSIX-complete musl OS port: VFS syscalls, signals, futex/threading, sockets, fork/exec, real clocks, and secure randomness still need kernel implementations.

## Tests

Fast build/structure test, no QEMU or musl download required:

```sh
make test-host
```

This builds a ring-3 ELF smoke program with no libc and checks:

- all kernel C/assembly builds under `-Werror`
- the userspace image is an x86_64 `ET_EXEC`
- it contains loadable ELF segments
- the high userspace entry point is correct
- the Limine request section is present in the kernel
- the fast syscall entry and C dispatcher are linked

Boot-test the kernel syscall ABI without musl:

```sh
make test-qemu-smoke
```

Boot-test the real musl program:

```sh
make test-qemu
```

The musl QEMU test asserts the serial log contains the userspace hello, malloc/string test, stdio test, and clean userspace exit.

A full `libc-test` run is not claimed yet. Upstream recommends `libc-test` for libc ports, but running its functional suite meaningfully requires substantially more kernel API surface than this starter kernel currently provides.

## Why not patch musl to use `int 0x80`?

musl's x86_64 port already speaks the Linux x86_64 syscall ABI with the `syscall` instruction. Keeping musl unmodified is more useful: the kernel implements the ABI boundary, while user programs remain ordinary static musl executables.

## License

The template code is provided under the repository license. musl is downloaded from upstream and keeps its own MIT license/copyright terms.
