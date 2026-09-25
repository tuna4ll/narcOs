# narcOs

A small POSIX-like hobby OS for x86_64, AArch64 and RISC-V 64.

```sh
make ARCH=x86_64 run
```

Use `aarch64` or `riscv64` to target another architecture.

## Native userspace ABI

`libnarc` is the native userspace-to-kernel interface. Its public API lives in
`lib/libnarc/include`, and the build produces `build/<arch>/lib/libnarc.a`.
The initial API covers ABI discovery, process identity and yielding, plus basic
file I/O:

```c
#include <narcos/narc.h>

static const char path[] = "/etc/motd";
narc_result_t result = narc_open(path, sizeof(path) - 1, NARC_OPEN_READ);
if (result.status != NARC_OK) {
    return 1;
}
```

Native syscall IDs are architecture-independent. Calls return their value and
status in separate registers, represented by `narc_result_t`. User programs
are freestanding binaries linked directly with `libnarc`; no external libc is
part of the build.
