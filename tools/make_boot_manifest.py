#!/usr/bin/env python3
import argparse
import struct
import sys
import zlib


BOOT_MANIFEST_MAGIC = 0x4D43524E
BOOT_MANIFEST_VERSION = 1
BOOT_MANIFEST_SIZE = 64
ELF_PT_LOAD = 1


def read_u16(data, off):
    return struct.unpack_from("<H", data, off)[0]


def read_u32(data, off):
    return struct.unpack_from("<I", data, off)[0]


def read_u64(data, off):
    return struct.unpack_from("<Q", data, off)[0]


def required_boot_size(data):
    if len(data) < 64 or data[:4] != b"\x7fELF":
        raise RuntimeError("kernel is not an ELF image")
    if data[5] != 1:
        raise RuntimeError("only little-endian ELF images are supported")

    elf_class = data[4]
    if elf_class == 1:
        if len(data) < 52:
            raise RuntimeError("truncated ELF32 header")
        phoff = read_u32(data, 28)
        phentsize = read_u16(data, 42)
        phnum = read_u16(data, 44)
        if phentsize < 32:
            raise RuntimeError("invalid ELF32 program header size")
        size = phoff + phentsize * phnum
        for idx in range(phnum):
            off = phoff + idx * phentsize
            if off + 32 > len(data):
                raise RuntimeError("ELF32 program header table exceeds file")
            if read_u32(data, off + 0) != ELF_PT_LOAD:
                continue
            file_off = read_u32(data, off + 4)
            file_size = read_u32(data, off + 16)
            size = max(size, file_off + file_size)
    elif elf_class == 2:
        phoff = read_u64(data, 32)
        phentsize = read_u16(data, 54)
        phnum = read_u16(data, 56)
        if phentsize < 56:
            raise RuntimeError("invalid ELF64 program header size")
        size = phoff + phentsize * phnum
        for idx in range(phnum):
            off = phoff + idx * phentsize
            if off + 56 > len(data):
                raise RuntimeError("ELF64 program header table exceeds file")
            if read_u32(data, off + 0) != ELF_PT_LOAD:
                continue
            file_off = read_u64(data, off + 8)
            file_size = read_u64(data, off + 32)
            size = max(size, file_off + file_size)
    else:
        raise RuntimeError("unsupported ELF class")

    if size <= 0 or size > len(data):
        raise RuntimeError("computed boot size is outside the ELF file")
    return size


def main():
    parser = argparse.ArgumentParser(description="Build a NarcOs boot manifest.")
    parser.add_argument("kernel")
    parser.add_argument("output")
    parser.add_argument("kernel_lba", type=int)
    parser.add_argument("--initrd-lba", type=int, default=0)
    parser.add_argument("--initrd-sectors", type=int, default=0)
    parser.add_argument("--initrd-size", type=int, default=0)
    parser.add_argument("--initrd-crc32", type=lambda value: int(value, 0), default=0)
    args = parser.parse_args()

    data = open(args.kernel, "rb").read()
    boot_size = required_boot_size(data)
    boot_data = data[:boot_size]
    sectors = (boot_size + 511) // 512

    buf = bytearray(512)
    struct.pack_into(
        "<IHHIIIIIIIII",
        buf,
        0,
        BOOT_MANIFEST_MAGIC,
        BOOT_MANIFEST_VERSION,
        BOOT_MANIFEST_SIZE,
        0,
        args.kernel_lba,
        sectors,
        boot_size,
        zlib.crc32(boot_data) & 0xFFFFFFFF,
        args.initrd_lba,
        args.initrd_sectors,
        args.initrd_size,
        args.initrd_crc32,
    )
    with open(args.output, "wb") as handle:
        handle.write(buf)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"[ERR] {exc}", file=sys.stderr)
        sys.exit(1)
