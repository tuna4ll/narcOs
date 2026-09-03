#!/bin/sh
set -eu

VERSION=${MUSL_VERSION:-1.2.6}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SRC="$ROOT/third_party/musl-$VERSION"
OBJ="$ROOT/build/musl/obj"
SYSROOT="$ROOT/build/musl/sysroot"
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '1')}

[ -x "$SRC/configure" ] || { echo "error: musl source missing; run scripts/fetch-musl.sh" >&2; exit 1; }

rm -rf "$OBJ" "$SYSROOT"
mkdir -p "$OBJ" "$SYSROOT"
cd "$OBJ"

CC=${CC:-cc} "$SRC/configure" \
    --prefix="$SYSROOT" \
    --syslibdir="$SYSROOT/lib" \
    --disable-shared \
    --enable-wrapper=gcc

make -j"$JOBS"
make install

test -x "$SYSROOT/bin/musl-gcc"
test -f "$SYSROOT/lib/libc.a"
test -f "$SYSROOT/include/stdio.h"
