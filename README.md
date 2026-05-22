<img width="842" height="245" alt="narcos-final_main" src="https://github.com/user-attachments/assets/4102a490-aec2-40c0-9429-bd1ede27c000" />

# NarcOs

A simple hobby operating system. It can run as `i386` or `x86_64`.
<img width="1899" height="871" alt="image (60)" src="https://github.com/user-attachments/assets/73aa791e-1930-4a9a-a202-c2465ecc7ce9" />

## Requirements

- `gcc`
- `ld`
- `nasm`
- `objcopy`
- `qemu-system-i386`
- `qemu-system-x86_64`

## Run 32-bit

```bash
make run-i386
```

Run with networking:

```bash
make run-net-i386
```

Build and boot the 32-bit ISO:

```bash
make iso-i386
make run-iso-i386
```

Test the same ISO as a Rufus/DD USB image in QEMU:

```bash
make run-iso-usb-i386
```

Build a 32-bit raw disk image explicitly:

```bash
make usb-i386
```

## Run 64-bit

```bash
make run-x86_64
```

Run with networking:

```bash
make run-x86_64-net
```

Build and boot the 64-bit ISO:

```bash
make iso-x86_64
make run-iso-x86_64
```

Test the same ISO as a Rufus/DD USB image in QEMU:

```bash
make run-iso-usb-x86_64
```

Build a 64-bit raw disk image explicitly:

```bash
make usb-x86_64
```

The ISO targets are hybrid images. They still boot as CD-ROM images in a VM,
but the same `.iso` can also be written directly to a USB drive with Rufus or
`dd`. If Rufus asks for write mode, choose `DD Image mode`. CD-ROM boot is
read-only and uses a volatile RAM filesystem overlay;
USB boot through Rufus/DD exposes the embedded NarcOs disk image as a writable
partition, so filesystem writes persist on the USB drive.

Current boot support is BIOS/legacy or UEFI-CSM. UEFI-only boot requires an EFI
bootloader and is not supported by this image yet.

## Clean

```bash
make clean
```
