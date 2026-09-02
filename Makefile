CC      ?= cc
LD      ?= ld
OBJCOPY ?= objcopy

BUILD := build
DIST  := dist

CFLAGS := -std=gnu11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector \
          -fno-pic -fno-pie -m64 -mno-red-zone -mcmodel=kernel -mno-sse -mno-sse2 \
          -I kernel/include
ASFLAGS := -ffreestanding -fno-pic -fno-pie -m64 -mno-red-zone -mcmodel=kernel
USER_CFLAGS := -std=gnu11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector \
               -fno-pic -fno-pie -m64 -mno-red-zone -mcmodel=large -mno-sse -mno-sse2 -I userland/include

KERNEL_C := $(shell find kernel -name '*.c' | sort)
KERNEL_S := $(filter-out kernel/user_blob.S,$(shell find kernel -name '*.S' | sort))
KERNEL_O := $(patsubst %.c,$(BUILD)/%.o,$(KERNEL_C)) $(patsubst %.S,$(BUILD)/%.o,$(KERNEL_S)) $(BUILD)/kernel/user_blob.o
USER_C   := $(shell find userland -name '*.c' | sort)
USER_O   := $(patsubst %.c,$(BUILD)/%.o,$(USER_C))

.PHONY: all kernel userland iso run run-serial clean distclean limine
all: iso

$(BUILD)/userland/%.o: userland/%.c
	@mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD)/userland.elf: $(USER_O) userland/linker.ld
	$(LD) -nostdlib -static -z max-page-size=0x1000 -T userland/linker.ld $(USER_O) -o $@

$(BUILD)/userland.bin: $(BUILD)/userland.elf
	$(OBJCOPY) -O binary $< $@

userland: $(BUILD)/userland.bin

$(BUILD)/kernel/%.o: kernel/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel/%.o: kernel/%.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/kernel/user_blob.o: kernel/user_blob.S $(BUILD)/userland.bin
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(KERNEL_O) kernel/linker.ld
	$(LD) -nostdlib -static -z max-page-size=0x1000 -T kernel/linker.ld $(KERNEL_O) -o $@

kernel: $(BUILD)/kernel.elf

limine:
	./scripts/fetch-limine.sh

iso: $(BUILD)/kernel.elf limine
	@command -v xorriso >/dev/null || { echo 'error: xorriso is required'; exit 1; }
	rm -rf $(BUILD)/iso_root $(DIST)
	mkdir -p $(BUILD)/iso_root/boot/limine $(BUILD)/iso_root/EFI/BOOT $(DIST)
	cp $(BUILD)/kernel.elf $(BUILD)/iso_root/boot/kernel.elf
	cp limine.conf $(BUILD)/iso_root/boot/limine/limine.conf
	cp limine/limine-bios.sys limine/limine-bios-cd.bin limine/limine-uefi-cd.bin $(BUILD)/iso_root/boot/limine/
	cp limine/BOOTX64.EFI $(BUILD)/iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J \
		-b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table \
		-hfsplus -apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(BUILD)/iso_root -o $(DIST)/kernel-template.iso
	./limine/limine bios-install $(DIST)/kernel-template.iso

run: iso
	@command -v qemu-system-x86_64 >/dev/null || { echo 'error: qemu-system-x86_64 is required'; exit 1; }
	qemu-system-x86_64 -M q35 -m 256M -vga std -cdrom $(DIST)/kernel-template.iso \
		-serial none -monitor none -no-reboot -no-shutdown

run-serial: iso
	@command -v qemu-system-x86_64 >/dev/null || { echo 'error: qemu-system-x86_64 is required'; exit 1; }
	qemu-system-x86_64 -M q35 -m 256M -vga std -cdrom $(DIST)/kernel-template.iso \
		-serial stdio -monitor none -no-reboot -no-shutdown

clean:
	rm -rf $(BUILD) $(DIST)

distclean: clean
	rm -rf limine
