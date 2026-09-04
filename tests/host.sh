#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

for test_script in tests/*.sh; do
    sh -n "$test_script"
done

test -f recipes/musl/RECIPE
test -d recipes/musl/patches
test -f recipes/limine/RECIPE
test -d recipes/limine/patches
test ! -d scripts

# Parse the Make database without downloading or building dependencies.
make -qp 2>/dev/null | grep -q '^musl:'
make -qp 2>/dev/null | grep -q '^limine:'

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
    0x4*) ;;
    *) echo "error: smoke entry is outside the expected low userspace image: $entry" >&2; exit 1 ;;
esac

echo '[host-test] PASS: recipes, userspace ELF, kernel link, SYSCALL entry'
