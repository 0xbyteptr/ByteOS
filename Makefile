CC = gcc
LD = gcc
AS = gcc
OBJCOPY = objcopy

BIN_DIR = bin
OUT_DIR = out

CFLAGS = -m64 -mcmodel=kernel -ffreestanding -fno-builtin -fno-stack-protector \
         -fno-pic -fno-pie -O2 -Wall -Wextra -I src

LDFLAGS = -nostdlib -static -no-pie \
          -T linker.ld -z max-page-size=0x1000 -mcmodel=kernel

# core sources + dynamically include any drivers in src/drivers
DRIVER_SRCS := $(shell find src/drivers -type f -name '*.c' 2>/dev/null)
SRCS = src/kernel/main.c \
       src/lib/stb_image_impl.c \
       src/lib/libc.c \
       src/mem/alloc.c \
	   src/mem/paging.c \
       src/assets/font.c \
       src/assets/bg.c \
       src/compat/compat.c \
       src/compat/stack_chk.c \
       src/compat/panic.c \
       src/graphics/framebuffer.c \
       src/graphics/font.c \
       src/console/console.c \
       src/multitasking/scheduler.c \
       src/gui/mia.c \
       src/gui/wm.c \
       src/userspace/enter_user.c \
       src/userspace/umain.c \
       src/shell/shell.c \
       src/syscall/syscall.c \
       src/boot/gdt.c \
       src/boot/tss.c \
       src/boot/idt.c \
       src/boot/limine_reqs.c \
       src/serial/serial.c \
       src/utils/log.c \
       $(DRIVER_SRCS)

# object files live under $(BIN_DIR) mirroring src/ paths
OBJS = $(patsubst src/%.c,$(BIN_DIR)/%.o,$(SRCS)) $(BIN_DIR)/boot/entry.o
BYTEBOX_BIN = apps/bytebox
BYTEBOX_OBJ = $(BIN_DIR)/assets/bytebox.o
OBJS += $(BYTEBOX_OBJ)

ISO = byteos.iso
ISO_DIR = isodir

.PHONY: all clean iso run fetch-stb

all: $(BIN_DIR)/kernel.elf


# generate C array from PSF font in assets/
src/assets/font.c: assets/default8x16.psf
	@echo "Generating C array for default8x16.psf"
	xxd -i $< | sed -E -e 's/unsigned char .*[dD]efault8x16_psf\[\]/extern const unsigned char assets_font_psf1[]/' -e 's/unsigned int .*[dD]efault8x16_psf_len/extern const unsigned int assets_font_psf1_len/' > $@; \

fetch-stb:
	@if [ ! -f src/lib/stb_image_stub.h ]; then \
		curl -fsSL https://raw.githubusercontent.com/nothings/stb/master/stb_image.h \
		-o src/lib/stb_image_stub.h ; \
	fi

# build rule: compile src/XYZ.c -> bin/XYZ.o
$(BIN_DIR)/%.o: src/%.c
	$(MAKE) fetch-stb
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# assembly -> object in bin/
$(BIN_DIR)/boot/entry.o: src/boot/entry.S
	@mkdir -p $(dir $@)
	$(AS) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/boot/gdt_asm.o: src/boot/gdt.S
	@mkdir -p $(dir $@)
	$(AS) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/boot/idt_asm.o: src/boot/idt.S
	@mkdir -p $(dir $@)
	$(AS) $(CFLAGS) -c $< -o $@

# multitasking context assembly
$(BIN_DIR)/multitasking/context.o: src/multitasking/context.S
	@mkdir -p $(dir $@)
	$(AS) $(CFLAGS) -c $< -o $@

# build bytebox
$(BYTEBOX_BIN): apps/bytebox.c
	$(CC) $(CFLAGS) -static $< -o $@


# embed bytebox binary as an object
$(BYTEBOX_OBJ): $(BYTEBOX_BIN)
	@mkdir -p $(dir $@)
	cp $(BYTEBOX_BIN) $(BIN_DIR)/assets/bytebox.tmp
	$(OBJCOPY) -I binary -O elf64-x86-64 -B i386:x86-64 $(BIN_DIR)/assets/bytebox.tmp $@
	rm -f $(BIN_DIR)/assets/bytebox.tmp

# embed toybox binary as an object
TOYBOX_BIN = extern/toybox
TOYBOX_OBJ = $(BIN_DIR)/assets/toybox.o
OBJS += $(TOYBOX_OBJ)

$(TOYBOX_OBJ): $(TOYBOX_BIN)
	@mkdir -p $(dir $@)
	cp $(TOYBOX_BIN) $(BIN_DIR)/assets/toybox.tmp
	$(OBJCOPY) -I binary -O elf64-x86-64 -B i386:x86-64 $(BIN_DIR)/assets/toybox.tmp $@
	rm -f $(BIN_DIR)/assets/toybox.tmp

# link into bin/kernel.elf
$(BIN_DIR)/kernel.elf: linker.ld $(OBJS) $(BIN_DIR)/multitasking/context.o $(BIN_DIR)/boot/gdt_asm.o $(BIN_DIR)/boot/idt_asm.o
	@mkdir -p $(BIN_DIR)
	$(LD) $(LDFLAGS) $(OBJS) $(BIN_DIR)/multitasking/context.o $(BIN_DIR)/boot/gdt_asm.o $(BIN_DIR)/boot/idt_asm.o -o $@

iso: $(BIN_DIR)/kernel.elf
	@echo "Building Limine ISO into $(OUT_DIR)/$(ISO)..."
	@rm -rf $(ISO_DIR) $(OUT_DIR)/$(ISO)
	@mkdir -p $(ISO_DIR)/boot $(OUT_DIR)

	@cp $(BIN_DIR)/kernel.elf $(ISO_DIR)/boot/kernel.elf
	@cp limine.conf $(ISO_DIR)/
	@cp limine/limine-bios.sys $(ISO_DIR)/
	@cp limine/limine-bios-cd.bin $(ISO_DIR)/

	# Ensure /bin directory exists in ISO and copy bytebox as /bin/sh and toybox as /bin/toybox
	@mkdir -p $(ISO_DIR)/bin
	@cp extern/toybox $(ISO_DIR)/bin/toybox
	@cp extern/toybox $(ISO_DIR)/bin/sh

	xorriso -as mkisofs \
	  -b limine-bios-cd.bin \
	  -no-emul-boot \
	  -boot-load-size 4 \
	  -boot-info-table \
	  -o $(OUT_DIR)/$(ISO) \
	  $(ISO_DIR)

	@echo "ISO ready: $(OUT_DIR)/$(ISO)"

run: iso
	@echo "Booting ISO with QEMU (Q35 machine, IDE CD, serial stdio)."
	qemu-system-x86_64 -m 1G -machine q35 -drive file=$(OUT_DIR)/$(ISO),if=ide,media=cdrom -boot order=d,once=d -serial stdio -no-reboot

clean:
	rm -rf $(BIN_DIR) $(OUT_DIR) $(ISO_DIR)
