CC      ?= cc
LD      ?= ld
USERLAND ?= musl

BUILD := build
DIST  := dist
MUSL_VERSION ?= 1.2.6
MUSL_SYSROOT := $(abspath $(BUILD)/musl/sysroot)
MUSL_CC := $(MUSL_SYSROOT)/bin/musl-gcc
USER_APP := $(BUILD)/userland/app
USER_MODE_STAMP := $(BUILD)/userland/.mode-$(USERLAND)
USER_BASE := 0x0000000000400000

CFLAGS := -std=gnu11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector \
          -fno-pic -fno-pie -m64 -mno-red-zone -mcmodel=kernel -mno-sse -mno-sse2 \
          -I kernel/include
ASFLAGS := -ffreestanding -fno-pic -fno-pie -m64 -mno-red-zone -mcmodel=kernel
USER_LINK_FLAGS := -nostdlib -static -Wl,-no-pie -Wl,-e,_start \
                   -Wl,-Ttext-segment=$(USER_BASE) -Wl,-z,max-page-size=0x1000 -Wl,--build-id=none
MUSL_USER_FLAGS := -std=c11 -O2 -Wall -Wextra -Werror -static -fno-pie -no-pie \
                   -Wl,-Ttext-segment=$(USER_BASE) -Wl,-z,max-page-size=0x1000 -Wl,--build-id=none

KERNEL_C := $(shell find kernel -name '*.c' | sort)
KERNEL_S := $(filter-out kernel/user_blob.S,$(shell find kernel -name '*.S' | sort))
KERNEL_O := $(patsubst %.c,$(BUILD)/%.o,$(KERNEL_C)) $(patsubst %.S,$(BUILD)/%.o,$(KERNEL_S)) $(BUILD)/kernel/user_blob.o

.PHONY: all kernel userland musl iso run run-serial clean distclean limine test test-host test-qemu test-qemu-smoke
all: iso

$(USER_MODE_STAMP):
	@mkdir -p $(dir $@)
	rm -f $(USER_APP) $(BUILD)/userland/.mode-*
	@touch $@

ifeq ($(USERLAND),musl)
$(MUSL_CC): scripts/fetch-musl.sh scripts/build-musl.sh
	MUSL_VERSION=$(MUSL_VERSION) ./scripts/fetch-musl.sh
	MUSL_VERSION=$(MUSL_VERSION) CC=$(CC) ./scripts/build-musl.sh

musl: $(MUSL_CC)

$(USER_APP): userland/hello.c $(MUSL_CC) $(USER_MODE_STAMP)
	@mkdir -p $(dir $@)
	$(MUSL_CC) $(MUSL_USER_FLAGS) $< -o $@
else ifeq ($(USERLAND),smoke)
$(USER_APP): tests/smoke.c $(USER_MODE_STAMP)
	@mkdir -p $(dir $@)
	$(CC) -std=gnu11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector \
		-fno-pie -m64 -mno-red-zone -mcmodel=large $(USER_LINK_FLAGS) $< -o $@
else
$(error USERLAND must be 'musl' or 'smoke')
endif

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

test: test-host

test-host:
	./tests/host.sh

test-qemu-smoke:
	./tests/qemu.sh smoke

test-qemu:
	./tests/qemu.sh musl

clean:
	rm -rf $(BUILD) $(DIST)

distclean: clean
	rm -rf limine third_party .cache
