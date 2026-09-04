# kernel-template

Minimal x86_64 kernel template for Türk OSDev.

It boots with Limine, enters ring 3, loads a static ELF user program and uses upstream musl as libc. The kernel only implements the small Linux-compatible syscall surface needed by the current userland; it is not a Linux kernel or a complete POSIX environment.

## Layout

```text
kernel/
  arch/x86_64/   GDT, IDT, exceptions, ring 3 and SYSCALL/SYSRET
  mm/            page allocation and user mappings
  console.c      Limine framebuffer console
  syscall.c      minimal Linux x86_64 syscall compatibility
  user.c         ELF loader and initial userspace stack
userland/
  hello.c
recipes/
  limine/
    RECIPE
    patches/
  musl/
    RECIPE
    patches/
```

## Requirements

```sh
sudo apt install build-essential curl xorriso qemu-system-x86
```

## Run

```sh
make run
```

For serial output:

```sh
make run-serial
```

Expected output:

```text
[boot] Limine framebuffer ready
[kernel] ring 0 initialized
[user] entering ring 3
Hello from userspace
[kernel] userspace exited
```

## Build targets

```sh
make userland   # build musl and userland/hello.c
make kernel     # build kernel.elf
make iso        # build bootable ISO
make run        # boot in QEMU
make clean      # remove build output
make distclean  # also remove downloaded sources/cache
```

## Recipes

Third-party build logic lives in `recipes/<name>/RECIPE`. Patches, when needed, go in `recipes/<name>/patches/` and are applied in filename order.

Current dependencies:

- Limine 12.9.0
- musl 1.2.6

## Userspace

`userland/hello.c` is linked as a static musl executable around `0x400000`. The kernel loads its ELF `PT_LOAD` segments, builds a Linux-style initial stack/auxv and enters CPL3.

The syscall path uses the native x86_64 `SYSCALL/SYSRET` mechanism. There is no legacy `int 0x80` compatibility path.

Currently implemented kernel-side calls include console I/O, anonymous memory mappings, `arch_prctl` for musl TLS, `set_tid_address`, basic stdio descriptor handling and process exit. Unsupported Linux syscalls return `-ENOSYS`.

## License

Repository code is covered by `LICENSE`. Limine and musl keep their upstream licenses.
