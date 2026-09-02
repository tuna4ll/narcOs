#!/bin/sh
set -eu

LIMINE_VERSION="${LIMINE_VERSION:-12.9.0}"
LIMINE_SHA256="84059c93b4ea03994af6d614654c7095291388850ea7b258d64f9263abde5557"
ARCHIVE="build/limine-binary-${LIMINE_VERSION}.tar.gz"
URL="https://github.com/Limine-Bootloader/Limine/releases/download/v${LIMINE_VERSION}/limine-binary.tar.gz"

# The Limine git repository is source-only and needs bootstrap/configure before
# it has a Makefile. For a kernel template, the official binary release is both
# faster and less fragile, and still builds the host-side `limine` utility
# locally.
if [ -x limine/limine ] && [ -f limine/limine-bios.sys ] && [ -f limine/limine-uefi-cd.bin ]; then
    exit 0
fi

rm -rf limine limine-binary
mkdir -p build

if [ ! -f "$ARCHIVE" ]; then
    command -v curl >/dev/null 2>&1 || {
        echo 'error: curl is required to fetch Limine' >&2
        exit 1
    }
    curl -fL "$URL" -o "$ARCHIVE"
fi

if command -v sha256sum >/dev/null 2>&1; then
    printf '%s  %s\n' "$LIMINE_SHA256" "$ARCHIVE" | sha256sum -c -
fi

tar -xzf "$ARCHIVE"
mv limine-binary limine

# The binary release ships the boot files ready to use; this builds only the
# small host-side installer used by `limine bios-install`.
make -C limine
