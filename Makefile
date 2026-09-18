CC ?= cc
LD ?= ld

BUILD := build
DIST := dist
CACHE := .cache
USER_APP := $(BUILD)/userland/app
USER_BASE := 0x400000

CFLAGS := -std=gnu11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector \
          -fno-pic -fno-pie -m64 -mno-red-zone -mcmodel=kernel -mno-sse -mno-sse2 \
          -I kernel/include
ASFLAGS := -ffreestanding -fno-pic -fno-pie -m64 -mno-red-zone -mcmodel=kernel
USER_CFLAGS := -std=c11 -O2 -Wall -Wextra -Werror -static -fno-pie -no-pie \
               -march=x86-64 -mtune=generic \
               -Wl,-Ttext-segment=$(USER_BASE) -Wl,-z,max-page-size=0x1000 -Wl,--build-id=none

KERNEL_C := $(shell find kernel -name '*.c' | sort)
KERNEL_S := $(filter-out kernel/user_blob.S,$(shell find kernel -name '*.S' | sort))
KERNEL_O := $(patsubst %.c,$(BUILD)/%.o,$(KERNEL_C)) \
            $(patsubst %.S,$(BUILD)/%.o,$(KERNEL_S)) \
            $(BUILD)/kernel/user_blob.o

.PHONY: all kernel userland iso run run-serial clean distclean
all: iso

include recipes/musl/RECIPE
include recipes/limine/RECIPE

$(USER_APP): userland/hello.c $(MUSL_CC)
	@mkdir -p $(dir $@)
	$(MUSL_CC) $(USER_CFLAGS) $< -o $@

userland: $(USER_APP)

$(BUILD)/kernel/%.o: kernel/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel/%.o: kernel/%.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/kernel/user_blob.o: kernel/user_blob.S $(USER_APP)
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(KERNEL_O) kernel/linker.ld
	$(LD) -nostdlib -static -z max-page-size=0x1000 -T kernel/linker.ld $(KERNEL_O) -o $@

kernel: $(BUILD)/kernel.elf

iso: $(BUILD)/kernel.elf $(LIMINE_TOOL)
	rm -rf $(BUILD)/iso_root $(DIST)
	mkdir -p $(BUILD)/iso_root/boot/limine $(BUILD)/iso_root/EFI/BOOT $(DIST)
	cp $(BUILD)/kernel.elf $(BUILD)/iso_root/boot/kernel.elf
	cp limine.conf $(BUILD)/iso_root/boot/limine/limine.conf
	cp $(LIMINE_SRC)/limine-bios.sys $(LIMINE_SRC)/limine-bios-cd.bin $(LIMINE_SRC)/limine-uefi-cd.bin $(BUILD)/iso_root/boot/limine/
	cp $(LIMINE_SRC)/BOOTX64.EFI $(BUILD)/iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J \
		-b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table \
		-hfsplus -apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(BUILD)/iso_root -o $(DIST)/narcOs.iso
	$(LIMINE_TOOL) bios-install $(DIST)/narcOs.iso

run: iso
	qemu-system-x86_64 -M q35 -m 256M -vga std -cdrom $(DIST)/narcOs.iso \
		-serial none -monitor none -no-reboot -no-shutdown

run-serial: iso
	qemu-system-x86_64 -M q35 -m 256M -vga std -cdrom $(DIST)/narcOs.iso \
		-serial stdio -monitor none -no-reboot -no-shutdown

clean:
	rm -rf $(BUILD) $(DIST)

distclean: clean
	rm -rf third_party $(CACHE)
