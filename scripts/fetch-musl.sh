#!/bin/sh
set -eu

VERSION=${MUSL_VERSION:-1.2.6}
SHA256=${MUSL_SHA256:-d585fd3b613c66151fc3249e8ed44f77020cb5e6c1e635a616d3f9f82460512a}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CACHE="$ROOT/.cache/musl"
THIRD="$ROOT/third_party"
DEST="$THIRD/musl-$VERSION"
ARCHIVE="$CACHE/musl-$VERSION.tar.gz"
URL="https://musl.libc.org/releases/musl-$VERSION.tar.gz"

if [ -d "$DEST" ] && [ -x "$DEST/configure" ]; then
    exit 0
fi

mkdir -p "$CACHE" "$THIRD"

if [ -n "${MUSL_TARBALL:-}" ]; then
    cp "$MUSL_TARBALL" "$ARCHIVE"
elif [ ! -f "$ARCHIVE" ]; then
    command -v curl >/dev/null 2>&1 || { echo 'error: curl is required to fetch musl' >&2; exit 1; }
    curl -fL --retry 3 --connect-timeout 15 "$URL" -o "$ARCHIVE"
fi

printf '%s  %s\n' "$SHA256" "$ARCHIVE" | sha256sum -c -
rm -rf "$DEST" "$THIRD/.musl-extract"
mkdir -p "$THIRD/.musl-extract"
tar -xzf "$ARCHIVE" -C "$THIRD/.musl-extract"
mv "$THIRD/.musl-extract/musl-$VERSION" "$DEST"
rmdir "$THIRD/.musl-extract"
