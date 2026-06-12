#!/usr/bin/env python3
import argparse
import shlex
import sys
from pathlib import Path


def ninja_quote(value):
    return shlex.quote(str(value))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", required=True)
    parser.add_argument("--build-root", required=True)
    parser.add_argument("--cc", default="gcc")
    parser.add_argument("--ld", default="ld")
    parser.add_argument("--nasm", default="nasm")
    parser.add_argument("--objcopy", default="objcopy")
    parser.add_argument("--vbe-width", default="1024")
    parser.add_argument("--vbe-height", default="768")
    parser.add_argument("--disk-image-sectors", default="49152")
    parser.add_argument("--musl-dir", default="user/ports/musl")
    parser.add_argument("--musl-prefix", default="/usr")
    parser.add_argument("--autorun-posix-smoke", action="store_true")
    args = parser.parse_args()

    source_root = Path(args.source_root).resolve()
    build_root = Path(args.build_root).resolve()
    ninja_file = build_root / "build.ninja"
    if not ninja_file.exists():
        return

    cmd = [
        sys.executable,
        source_root / "tools/meson_clean.py",
        "--source-root", source_root,
        "--build-root", build_root,
        "--cc", args.cc,
        "--ld", args.ld,
        "--nasm", args.nasm,
        "--objcopy", args.objcopy,
        "--vbe-width", args.vbe_width,
        "--vbe-height", args.vbe_height,
        "--disk-image-sectors", args.disk_image_sectors,
        "--musl-dir", args.musl_dir,
        "--musl-prefix", args.musl_prefix,
    ]
    if args.autorun_posix_smoke:
        cmd.append("--autorun-posix-smoke")
    command_line = " COMMAND = " + " ".join(ninja_quote(part) for part in cmd) + "\n"

    lines = ninja_file.read_text(encoding="utf-8").splitlines(keepends=True)
    changed = False
    in_clean = False
    for i, line in enumerate(lines):
        if line.startswith("build meson-internal__clean:"):
            in_clean = True
            continue
        if in_clean and line.startswith("build "):
            break
        if in_clean and line.startswith(" COMMAND = "):
            if lines[i] != command_line:
                lines[i] = command_line
                changed = True
            break

    if changed:
        ninja_file.write_text("".join(lines), encoding="utf-8")


if __name__ == "__main__":
    main()
