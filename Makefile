ARCH ?= x86_64

BUILD := build/$(ARCH)
DIST := dist/$(ARCH)
CACHE := .cache
USER_APP := $(BUILD)/userland/init
INITRAMFS := $(BUILD)/initramfs.tar
INITRAMFS_ROOT := $(BUILD)/initramfs_root
USER_BASE := 0x400000
ROOTFS_FILES := $(shell find userland/rootfs -type f 2>/dev/null | sort)

COMMON_CFLAGS := -std=gnu11 -O2 -Wall -Wextra -Werror -ffreestanding \
                 -fno-stack-protector -fno-pic -fno-pie -I kernel/include -I include
COMMON_USER_CFLAGS := -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding \
                      -fno-stack-protector -fno-pic -fno-pie -I include \
                      -I lib/libnarc/include
COMMON_USER_LDFLAGS := -nostdlib -static -Wl,-z,max-page-size=0x1000 \
                       -Wl,--build-id=none -Wl,-e,_start
LIBNARC_CFLAGS := -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding \
                  -fno-stack-protector -I include -I lib/libnarc/include

ifeq ($(ARCH),x86_64)
KCC := cc
KLD := ld
KERNEL_CFLAGS := -m64 -mno-red-zone -mcmodel=kernel -mno-sse -mno-sse2
KERNEL_ASFLAGS := -m64 -mno-red-zone -mcmodel=kernel
USER_ARCH_FLAGS := -march=x86-64 -mtune=generic
USER_LINK_FLAGS := -no-pie -Wl,-Ttext-segment=$(USER_BASE)
USER_CC := cc
USER_AR := ar
EFI_BOOT := BOOTX64.EFI
ISO_BIOS_FLAGS := -b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table
else ifeq ($(ARCH),aarch64)
KCC := aarch64-linux-gnu-gcc
KLD := aarch64-linux-gnu-ld
KERNEL_CFLAGS := -mgeneral-regs-only -mstrict-align
KERNEL_ASFLAGS :=
USER_ARCH_FLAGS := -march=armv8-a -fno-link-libatomic
USER_LINK_FLAGS := -no-pie -Wl,-Ttext-segment=$(USER_BASE)
USER_CC := aarch64-linux-gnu-gcc
USER_AR := aarch64-linux-gnu-ar
EFI_BOOT := BOOTAA64.EFI
else ifeq ($(ARCH),riscv64)
KCC := riscv64-linux-gnu-gcc
KLD := riscv64-linux-gnu-ld
KERNEL_CFLAGS := -march=rv64imac_zicsr_zifencei -mabi=lp64 -mcmodel=medany -mno-relax -msmall-data-limit=0
KERNEL_ASFLAGS := -march=rv64imac_zicsr_zifencei -mabi=lp64 -mcmodel=medany -mno-relax -msmall-data-limit=0
USER_ARCH_FLAGS := -march=rv64gc_zicsr_zifencei -mabi=lp64d \
                   -msmall-data-limit=0 -fno-link-libatomic
USER_LINK_FLAGS := -no-pie -Wl,-Ttext-segment=$(USER_BASE)
USER_CC := riscv64-linux-gnu-gcc
USER_AR := riscv64-linux-gnu-ar
EFI_BOOT := BOOTRISCV64.EFI
else
$(error unsupported ARCH: $(ARCH))
endif

CFLAGS := $(COMMON_CFLAGS) $(KERNEL_CFLAGS)
ASFLAGS := -ffreestanding -fno-pic -fno-pie $(KERNEL_ASFLAGS)
USER_CFLAGS := $(COMMON_USER_CFLAGS) $(USER_ARCH_FLAGS) $(USER_LINK_FLAGS)
USER_LDFLAGS := $(COMMON_USER_LDFLAGS) $(USER_LINK_FLAGS)
LINKER := kernel/arch/$(ARCH)/linker.ld
LIBNARC_COMMON_C := $(shell find lib/libnarc/src -name '*.c' | sort)
LIBNARC_ARCH_C := lib/libnarc/arch/$(ARCH)/syscall.c
LIBNARC_C := $(LIBNARC_COMMON_C) $(LIBNARC_ARCH_C)
LIBNARC_OBJ := $(patsubst lib/libnarc/%.c,$(BUILD)/lib/libnarc/%.o,$(LIBNARC_C))
LIBNARC_CRT := $(BUILD)/lib/libnarc/crt0.o
LIBNARC := $(BUILD)/lib/libnarc.a

COMMON_KERNEL_C := $(shell find kernel -path kernel/arch -prune -o -name '*.c' -print | sort)
COMMON_KERNEL_S := $(shell find kernel -path kernel/arch -prune -o -name '*.S' -print | sort)
ARCH_KERNEL_C := $(shell find kernel/arch/$(ARCH) -name '*.c' | sort)
ARCH_KERNEL_S := $(shell find kernel/arch/$(ARCH) -name '*.S' | sort)
KERNEL_C := $(COMMON_KERNEL_C) $(ARCH_KERNEL_C)
KERNEL_S := $(COMMON_KERNEL_S) $(ARCH_KERNEL_S)
KERNEL_O := $(patsubst %.c,$(BUILD)/%.o,$(KERNEL_C)) \
            $(patsubst %.S,$(BUILD)/%.o,$(KERNEL_S))

.PHONY: all kernel libnarc userland iso run run-serial clean distclean
all: iso

include recipes/limine/RECIPE
include recipes/edk2/RECIPE

$(BUILD)/lib/libnarc/%.o: lib/libnarc/%.c include/narcos/abi.h \
                         lib/libnarc/include/narcos/narc.h
	@mkdir -p $(dir $@)
	$(USER_CC) $(LIBNARC_CFLAGS) $(USER_ARCH_FLAGS) -c $< -o $@

$(LIBNARC_CRT): lib/libnarc/arch/$(ARCH)/crt0.S
	@mkdir -p $(dir $@)
	$(USER_CC) -ffreestanding -fno-pic -fno-pie $(USER_ARCH_FLAGS) -c $< -o $@

$(LIBNARC): $(LIBNARC_OBJ)
	@mkdir -p $(dir $@)
	$(RM) $@
	$(USER_AR) rcs $@ $^

libnarc: $(LIBNARC) $(LIBNARC_CRT)

$(USER_APP): userland/init.c $(LIBNARC) $(LIBNARC_CRT)
	@mkdir -p $(dir $@)
	$(USER_CC) $(USER_CFLAGS) $(LIBNARC_CRT) $< $(LIBNARC) $(USER_LDFLAGS) -o $@

userland: $(USER_APP)

$(INITRAMFS): $(USER_APP) $(ROOTFS_FILES)
	rm -rf $(INITRAMFS_ROOT)
	mkdir -p $(INITRAMFS_ROOT)/sbin $(INITRAMFS_ROOT)/bin
	cp -R userland/rootfs/. $(INITRAMFS_ROOT)/
	cp $(USER_APP) $(INITRAMFS_ROOT)/sbin/init
	cp $(USER_APP) $(INITRAMFS_ROOT)/bin/init
	tar --format=ustar --owner=0 --group=0 --numeric-owner -cf $@ -C $(INITRAMFS_ROOT) .

$(BUILD)/kernel/%.o: kernel/%.c
	@mkdir -p $(dir $@)
	$(KCC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel/%.o: kernel/%.S
	@mkdir -p $(dir $@)
	$(KCC) $(ASFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(KERNEL_O) $(LINKER)
	$(KLD) -nostdlib -static -z max-page-size=0x1000 -T $(LINKER) $(KERNEL_O) -o $@

kernel: $(BUILD)/kernel.elf

iso: $(BUILD)/kernel.elf $(INITRAMFS) $(LIMINE_TOOL)
	rm -rf $(BUILD)/iso_root $(DIST)
	mkdir -p $(BUILD)/iso_root/boot/limine $(BUILD)/iso_root/EFI/BOOT $(DIST)
	cp $(BUILD)/kernel.elf $(BUILD)/iso_root/boot/kernel.elf
	cp $(INITRAMFS) $(BUILD)/iso_root/boot/initramfs.tar
	cp limine.conf $(BUILD)/iso_root/boot/limine/limine.conf
	cp $(LIMINE_SRC)/limine-uefi-cd.bin $(BUILD)/iso_root/boot/limine/
	cp $(LIMINE_SRC)/$(EFI_BOOT) $(BUILD)/iso_root/EFI/BOOT/
	$(if $(filter x86_64,$(ARCH)),cp $(LIMINE_SRC)/limine-bios.sys $(LIMINE_SRC)/limine-bios-cd.bin $(BUILD)/iso_root/boot/limine/)
	xorriso -as mkisofs -R -r -J $(ISO_BIOS_FLAGS) \
		-hfsplus -apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(BUILD)/iso_root -o $(DIST)/narcOs-$(ARCH).iso
	$(if $(filter x86_64,$(ARCH)),$(LIMINE_TOOL) bios-install $(DIST)/narcOs-$(ARCH).iso)

ifeq ($(ARCH),x86_64)
run run-serial: iso
	qemu-system-x86_64 -M q35 -m 256M -vga std -cdrom $(DIST)/narcOs-$(ARCH).iso \
		-serial $(if $(filter run-serial,$@),stdio,none) -monitor none -no-reboot -no-shutdown
else ifeq ($(ARCH),aarch64)
run run-serial: iso $(EDK2_PREPARED)
	cp $(EDK2_DIR)/ovmf-vars-aarch64.fd $(BUILD)/ovmf-vars.fd
	qemu-system-aarch64 -M virt -cpu cortex-a72 -m 256M -device ramfb \
		-drive if=pflash,unit=0,format=raw,file=$(EDK2_DIR)/ovmf-code-aarch64.fd,readonly=on \
		-drive if=pflash,unit=1,format=raw,file=$(BUILD)/ovmf-vars.fd \
		-cdrom $(DIST)/narcOs-$(ARCH).iso -serial $(if $(filter run-serial,$@),stdio,none) \
		-monitor none -no-reboot -no-shutdown
else
run run-serial: iso $(EDK2_PREPARED)
	cp $(EDK2_DIR)/ovmf-vars-riscv64.fd $(BUILD)/ovmf-vars.fd
	qemu-system-riscv64 -M virt -cpu rv64 -m 256M -device ramfb \
		-drive if=pflash,unit=0,format=raw,file=$(EDK2_DIR)/ovmf-code-riscv64.fd,readonly=on \
		-drive if=pflash,unit=1,format=raw,file=$(BUILD)/ovmf-vars.fd \
		-cdrom $(DIST)/narcOs-$(ARCH).iso -serial $(if $(filter run-serial,$@),stdio,none) \
		-monitor none -no-reboot -no-shutdown
endif

clean:
	rm -rf build dist

distclean: clean
	rm -rf third_party $(CACHE)
