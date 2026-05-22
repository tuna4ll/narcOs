#!/usr/bin/env python3
import argparse
import os
import struct
import subprocess
import sys
import tempfile


ISO_SECTOR_SIZE = 2048
BOOT_RECORD_LBA = 17
BOOT_CATALOG_OFFSET = 71
EL_TORITO_SIGNATURE = b"EL TORITO SPECIFICATION"


def read_at(handle, offset, size):
    handle.seek(offset)
    data = handle.read(size)
    if len(data) != size:
        raise RuntimeError("unexpected end of ISO image")
    return data


def find_boot_image_lba_512(iso_path):
    with open(iso_path, "rb") as handle:
        boot_record = read_at(handle, BOOT_RECORD_LBA * ISO_SECTOR_SIZE, ISO_SECTOR_SIZE)
        if boot_record[0] != 0 or boot_record[1:6] != b"CD001":
            raise RuntimeError("ISO has no El Torito boot record at sector 17")
        if boot_record[7:7 + len(EL_TORITO_SIGNATURE)] != EL_TORITO_SIGNATURE:
            raise RuntimeError("ISO boot record is not El Torito")

        catalog_lba = struct.unpack_from("<I", boot_record, BOOT_CATALOG_OFFSET)[0]
        if catalog_lba == 0:
            raise RuntimeError("El Torito boot catalog LBA is zero")

        catalog = read_at(handle, catalog_lba * ISO_SECTOR_SIZE, ISO_SECTOR_SIZE)
        if catalog[0] != 0x01 or catalog[0x1E] != 0x55 or catalog[0x1F] != 0xAA:
            raise RuntimeError("El Torito validation entry is invalid")
        if catalog[0x20] != 0x88:
            raise RuntimeError("El Torito default boot entry is not bootable")

        boot_image_lba_2048 = struct.unpack_from("<I", catalog, 0x28)[0]
        if boot_image_lba_2048 == 0:
            raise RuntimeError("El Torito boot image LBA is zero")
        return boot_image_lba_2048 * (ISO_SECTOR_SIZE // 512)


def build_mbr(nasm, boot_asm, output_path, image_base_lba, disk_image_sectors):
    cmd = [
        nasm,
        "-f", "bin",
        f"-DDISK_BASE_LBA={image_base_lba}",
        f"-DPARTITION_START_LBA={image_base_lba}",
        f"-DPARTITION_SECTORS={disk_image_sectors}",
        f"-DDISK_IMAGE_SECTORS={disk_image_sectors}",
        boot_asm,
        "-o", output_path,
    ]
    subprocess.run(cmd, check=True)
    size = os.path.getsize(output_path)
    if size != 512:
        raise RuntimeError(f"hybrid MBR must be 512 bytes, got {size}")


def patch_iso_mbr(iso_path, mbr_path):
    with open(mbr_path, "rb") as handle:
        mbr = handle.read()
    if len(mbr) != 512:
        raise RuntimeError("hybrid MBR is not 512 bytes")
    with open(iso_path, "r+b") as handle:
        handle.seek(0)
        handle.write(mbr)


def main():
    parser = argparse.ArgumentParser(
        description="Patch a NarcOs El Torito ISO so the same file boots from Rufus/DD USB."
    )
    parser.add_argument("--iso", required=True)
    parser.add_argument("--boot-asm", required=True)
    parser.add_argument("--disk-image-sectors", required=True, type=int)
    parser.add_argument("--nasm", default="nasm")
    args = parser.parse_args()

    image_base_lba = find_boot_image_lba_512(args.iso)
    iso_sectors = (os.path.getsize(args.iso) + 511) // 512
    if image_base_lba + args.disk_image_sectors > iso_sectors:
        raise RuntimeError(
            f"embedded disk image exceeds ISO size: base={image_base_lba} "
            f"sectors={args.disk_image_sectors} iso_sectors={iso_sectors}"
        )

    tmp_dir = os.path.dirname(os.path.abspath(args.iso)) or "."
    fd, mbr_path = tempfile.mkstemp(prefix=".narcos-hybrid-", suffix=".bin", dir=tmp_dir)
    os.close(fd)
    try:
        build_mbr(args.nasm, args.boot_asm, mbr_path, image_base_lba, args.disk_image_sectors)
        patch_iso_mbr(args.iso, mbr_path)
    finally:
        try:
            os.unlink(mbr_path)
        except FileNotFoundError:
            pass

    print(
        f"[OK] Rufus hybrid MBR: {args.iso} "
        f"boot_image_lba={image_base_lba} sectors={args.disk_image_sectors}"
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"[ERR] {exc}", file=sys.stderr)
        sys.exit(1)
