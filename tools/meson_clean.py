#!/usr/bin/env python3
import argparse
import subprocess
import sys
from pathlib import Path


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

    subprocess.run(["ninja", "-t", "clean"], cwd=build_root, check=True)

    clean_cmd = [
        sys.executable,
        str(source_root / "tools/meson_build.py"),
        "--target", "clean-generated",
        "--source-root", str(source_root),
        "--build-root", str(build_root),
        "--cc", args.cc,
        "--ld", args.ld,
        "--nasm", args.nasm,
        "--objcopy", args.objcopy,
        "--vbe-width", str(args.vbe_width),
        "--vbe-height", str(args.vbe_height),
        "--disk-image-sectors", str(args.disk_image_sectors),
        "--musl-dir", args.musl_dir,
        "--musl-prefix", args.musl_prefix,
    ]
    if args.autorun_posix_smoke:
        clean_cmd.append("--autorun-posix-smoke")
    subprocess.run(clean_cmd, check=True)


if __name__ == "__main__":
    main()
