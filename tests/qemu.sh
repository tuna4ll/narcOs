#!/bin/sh
set -eu
MODE=${1:-musl}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

command -v qemu-system-x86_64 >/dev/null 2>&1 || { echo 'error: qemu-system-x86_64 is required' >&2; exit 1; }
command -v xorriso >/dev/null 2>&1 || { echo 'error: xorriso is required' >&2; exit 1; }

make clean >/dev/null
make USERLAND="$MODE" iso
LOG=$(mktemp)
trap 'rm -f "$LOG"' EXIT INT TERM

set +e
timeout 20s qemu-system-x86_64 -M q35 -m 256M -vga std \
    -cdrom dist/kernel-template.iso -display none -serial stdio -monitor none \
    -no-reboot -no-shutdown -device isa-debug-exit,iobase=0xf4,iosize=0x04 >"$LOG" 2>&1
status=$?
set -e
cat "$LOG"

# isa-debug-exit deliberately returns a non-zero host status; timeout (124) is failure.
[ "$status" -ne 124 ] || { echo '[qemu-test] FAIL: timed out' >&2; exit 1; }
grep -Fq '[boot] Limine framebuffer ready' "$LOG"
grep -Fq '[kernel] ring 0 initialized' "$LOG"
grep -Fq '[user] entering ring 3' "$LOG"

if [ "$MODE" = musl ]; then
    grep -Fq 'Hello from userspace' "$LOG"
    grep -Fq '[musl] malloc + string are alive' "$LOG"
    grep -Fq '[musl] stdio + malloc + string OK' "$LOG"
else
    grep -Fq '[smoke] syscall + ELF loader OK' "$LOG"
    grep -Fq '[smoke] mmap + writev + ioctl OK' "$LOG"
fi

grep -Fq '[kernel] userspace exited' "$LOG"
echo "[qemu-test] PASS: $MODE"
