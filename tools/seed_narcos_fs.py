#!/usr/bin/env python3
import argparse
import os
import struct
import sys


MAX_FILES = 64
NODE_SIZE = 64
DIR_SECTOR = 3072
DIR_SECTOR_COUNT = 8
DATA_START_SECTOR = 4096
DATA_END_SECTOR = 6144
SECTOR_SIZE = 512
FS_ROOT_INDEX = -1
FS_NODE_FILE = 1
FS_NODE_DIR = 2

README_TEXT = (
    b"Welcome to NarcOs Professional Desktop!\n"
    b"Files here appear on your desktop icons.\n"
)


def pack_node(name, size, lba, flags, parent, sector_count=0, extra_flags=0):
    name_bytes = name.encode("ascii")
    if len(name_bytes) > 31:
        raise ValueError(f"node name too long: {name}")
    reserved = struct.pack("<II", sector_count, extra_flags) + bytes(8)
    return struct.pack(
        "<32sIIIi16s",
        name_bytes,
        size,
        lba,
        flags,
        parent,
        reserved,
    )


def build_directory_table(files):
    nodes = [
        pack_node("bin", 0, 0, FS_NODE_DIR, FS_ROOT_INDEX),
        pack_node("assets", 0, 0, FS_NODE_DIR, FS_ROOT_INDEX),
        pack_node("system", 0, 0, FS_NODE_DIR, FS_ROOT_INDEX),
        pack_node("home", 0, 0, FS_NODE_DIR, FS_ROOT_INDEX),
        pack_node("user", 0, 0, FS_NODE_DIR, 3),
        pack_node("Desktop", 0, 0, FS_NODE_DIR, 4),
        pack_node(
            "readme.txt",
            len(README_TEXT),
            DATA_START_SECTOR,
            FS_NODE_FILE,
            5,
            sector_count=1,
        ),
    ]
    for file_entry in files:
        nodes.append(
            pack_node(
                file_entry["name"],
                file_entry["size"],
                file_entry["lba"],
                FS_NODE_FILE,
                0,
                sector_count=file_entry["sectors"],
            )
        )
    if len(nodes) > MAX_FILES:
        raise RuntimeError(f"too many filesystem nodes: {len(nodes)} > {MAX_FILES}")
    table = bytearray(DIR_SECTOR_COUNT * SECTOR_SIZE)
    for index, node in enumerate(nodes):
        table[index * NODE_SIZE:(index + 1) * NODE_SIZE] = node
    if len(table) != MAX_FILES * NODE_SIZE:
        raise RuntimeError("directory table size mismatch")
    return table


def collect_seed_files(bin_dir, programs):
    files = []
    next_lba = DATA_START_SECTOR + 1

    if not bin_dir or not programs:
        return files

    for program in programs:
        if not program:
            continue
        path = os.path.join(bin_dir, program)
        if not os.path.isfile(path):
            raise RuntimeError(f"missing seed binary: {path}")
        size = os.path.getsize(path)
        sectors = (size + SECTOR_SIZE - 1) // SECTOR_SIZE
        if next_lba + sectors > DATA_END_SECTOR:
            raise RuntimeError(f"seed filesystem data area full while adding {program}")
        files.append({
            "name": program,
            "path": path,
            "size": size,
            "lba": next_lba,
            "sectors": sectors,
        })
        next_lba += sectors
    return files


def seed_image(image_path, bin_dir=None, programs=None):
    min_size = (DATA_START_SECTOR + 1) * SECTOR_SIZE
    if os.path.getsize(image_path) < min_size:
        raise RuntimeError("disk image is too small for NarcOs filesystem seed")

    files = collect_seed_files(bin_dir, programs or [])
    dir_table = build_directory_table(files)
    readme_sector = bytearray(SECTOR_SIZE)
    readme_sector[:len(README_TEXT)] = README_TEXT

    with open(image_path, "r+b") as handle:
        handle.seek(DIR_SECTOR * SECTOR_SIZE)
        handle.write(dir_table)
        handle.seek(DATA_START_SECTOR * SECTOR_SIZE)
        handle.write(readme_sector)
        for file_entry in files:
            with open(file_entry["path"], "rb") as source:
                data = source.read()
            padded = data + bytes(file_entry["sectors"] * SECTOR_SIZE - len(data))
            handle.seek(file_entry["lba"] * SECTOR_SIZE)
            handle.write(padded)


def main():
    parser = argparse.ArgumentParser(description="Seed a NarcOs raw disk image with the initial FS.")
    parser.add_argument("image")
    parser.add_argument("--bin-dir", default=None)
    parser.add_argument("--programs", default="")
    args = parser.parse_args()
    programs = args.programs.split() if args.programs else []
    seed_image(args.image, args.bin_dir, programs)
    print(f"[OK] seeded NarcOs filesystem: {args.image}")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"[ERR] {exc}", file=sys.stderr)
        sys.exit(1)
