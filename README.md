<img width="842" height="245" alt="narcos-final_main" src="https://github.com/user-attachments/assets/4102a490-aec2-40c0-9429-bd1ede27c000" />

# NarcOs

A simple hobby operating system. It can run as `i386` or `x86_64`.
<img width="1900" height="965" alt="narcOs" src="https://github.com/user-attachments/assets/bc26f37a-f406-434c-ada8-bf1a441c582b" />


## Requirements

- `gcc`
- `ld`
- `nasm`
- `objcopy`
- `ar`
- `ranlib`
- `make` for the vendored musl build
- ImageMagick `magick`
- `genisoimage` for ISO targets
- `meson`
- `ninja`
- `qemu-system-i386`
- `qemu-system-x86_64`

Configure once:

```bash
meson setup build
```

## Run 32-bit

```bash
ninja -C build run-i386
```

Run with networking:

```bash
ninja -C build run-net-i386
```

Build and boot the 32-bit ISO:

```bash
ninja -C build iso-i386
ninja -C build run-iso-i386
```

Test the same ISO as a Rufus/DD USB image in QEMU:

```bash
ninja -C build run-iso-usb-i386
```

Build a 32-bit raw disk image explicitly:

```bash
ninja -C build usb-i386
```

## Run 64-bit

```bash
ninja -C build run-x86_64
```

Run with networking:

```bash
ninja -C build run-x86_64-net
```

Build and boot the 64-bit ISO:

```bash
ninja -C build iso-x86_64
ninja -C build run-iso-x86_64
```

Test the same ISO as a Rufus/DD USB image in QEMU:

```bash
ninja -C build run-iso-usb-x86_64
```

Build a 64-bit raw disk image explicitly:

```bash
ninja -C build usb-x86_64
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
ninja -C build clean-generated
ninja -C build clean
```
