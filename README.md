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

## Multi-Core Symmetric Multiprocessing (SMP) with Rust

narcOs supports Symmetric Multiprocessing (SMP) for `x86_64` using a modern, bare-metal `no_std` Rust static library (`narcos-smp`) integrated directly into the core hybrid C kernel.

### Architecture & Features

1. **Rust Integration:** The SMP subsystem (`crates/narcos-smp`) is compiled as a static library for `x86_64-unknown-none` and linked during the kernel build using `LDFLAGS` with garbage collection (`--gc-sections`) to minimize footprint.
2. **ACPI Parsing:** Traverses the Root System Description Pointer (RSDP) in high memory and parses the Multiple APIC Description Table (MADT) to retrieve core topologies and Local APIC addresses.
3. **Paging Integration:** Deferring initialization to post-paging allows parsing ACPI tables placed above the initial 4MB identity-mapped boundary.
4. **Dynamic MMIO Mapping:** Dynamically maps the Local APIC MMIO physical region (`0xFEE00000`) into virtual memory using the kernel's `paging_map_physical` API to avoid page faults under active paging.
5. **Core Awakening:** Wakes up inactive Application Processors (APs) by sending `INIT` and `STARTUP` Inter-Processor Interrupts (IPIs) through the Local APIC using the assembly trampoline vector.

### Observing Multi-Core Initialization

Run narcOs in QEMU with multiple cores (default `-smp 4`):

```bash
make run-x86_64-headless
```

Expected diagnostic serial outputs:

```text
[boot] initializing Rust SMP module...
[smp] starting rust-smp module initialization...
[acpi] entering parse_acpi_tables
[smp] acpi successfully parsed
[smp] physical lapic address: 0xFEE00000
[smp] mapping local apic mmio region...
[smp] local apic mapped to virtual address: 0x00A00000
[smp] local apic enabled on bsp core
[smp] multiple cpu cores detected! waking up APs...
[smp] triggered startup sequence on core
[boot] Rust SMP returned core_count=0x00000004
```
