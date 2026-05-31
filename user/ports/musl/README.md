# NarcOs musl port

This directory contains the vendored musl source tree plus the NarcOs-specific
overlay files needed for the static userland libc port.

Initial scope:

- static linking only
- i386 and x86_64 user executables
- `int 0x80` NarcOs syscall ABI
- no dynamic linker, shared libc, pthreads, fork, signals, or mmap-backed VM

The build applies the overlay automatically through the top-level Makefile.
To re-apply it manually after refreshing the musl source tree:

```sh
python3 tools/apply_musl_narcos_overlay.py user/ports/musl
```

The overlay is intentionally small. Most libc behavior still comes from musl;
NarcOs-specific work lives in `overlay/arch/*/syscall_arch.h`, `overlay/crt/*`,
and the syscall number/flag definitions under `overlay/include/narcos`.
