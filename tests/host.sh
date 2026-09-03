#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

for script in scripts/*.sh tests/*.sh; do
    sh -n "$script"
done

make clean >/dev/null
make USERLAND=smoke kernel

readelf -h build/userland/app | grep -q 'Type:.*EXEC'
readelf -h build/userland/app | grep -q 'Machine:.*Advanced Micro Devices X86-64'
readelf -l build/userland/app | grep -q 'LOAD'
readelf -S build/kernel.elf | grep -q '.limine_requests'
nm build/kernel.elf | grep -q ' syscall_fast_entry$'
nm build/kernel.elf | grep -q ' syscall_dispatch_fast$'

entry=$(readelf -h build/userland/app | awk '/Entry point address:/ {print $4}')
case "$entry" in
    0x1000*) ;;
    *) echo "error: smoke entry is outside the expected high userspace image: $entry" >&2; exit 1 ;;
esac

echo '[host-test] PASS: scripts, userspace ELF, kernel link, SYSCALL entry'
