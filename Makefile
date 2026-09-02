CC ?= cc
LD ?= ld
BUILD := build
DIST := dist
CFLAGS := -std=gnu11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m64 -mno-red-zone -mcmodel=kernel -mno-sse -mno-sse2 -I kernel/include

BASE_O := $(BUILD)/kernel/main.o $(BUILD)/kernel/serial.o $(BUILD)/kernel/lib/string.o

.PHONY: all kernel limine iso run clean distclean
all: iso

$(BUILD)/kernel/%.o: kernel/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(BASE_O) kernel/linker.ld
	$(LD) -nostdlib -static -z max-page-size=0x1000 -T kernel/linker.ld $(BASE_O) -o $@

kernel: $(BUILD)/kernel.elf

limine:
	./scripts/fetch-limine.sh

iso: $(BUILD)/kernel.elf limine
	@command -v xorriso >/dev/null || { echo 'error: xorriso is required'; exit 1; }
	rm -rf $(BUILD)/iso_root $(DIST)
	mkdir -p $(BUILD)/iso_root/boot/limine $(BUILD)/iso_root/EFI/BOOT $(DIST)
	cp $(BUILD)/kernel.elf $(BUILD)/iso_root/boot/kernel.elf
	cp limine.conf $(BUILD)/iso_root/boot/limine/limine.conf
	cp limine/limine-bios.sys limine/limine-bios-cd.bin $(BUILD)/iso_root/boot/limine/
	cp limine/BOOTX64.EFI $(BUILD)/iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus -apm-block-size 2048 --efi-boot EFI/BOOT/BOOTX64.EFI -efi-boot-part --efi-boot-image --protective-msdos-label $(BUILD)/iso_root -o $(DIST)/kernel-template.iso
	./limine/limine bios-install $(DIST)/kernel-template.iso

run: iso
	qemu-system-x86_64 -M q35 -m 256M -cdrom $(DIST)/kernel-template.iso -serial stdio -display none -no-reboot -no-shutdown

clean:
	rm -rf $(BUILD) $(DIST)

distclean: clean
	rm -rf limine
