ISO_DIR = iso
OUT_DIR = out
ISO = ByteOS.iso
KERNEL = build/kernel.elf

build:
	mkdir -p build
	cd build && cmake .. && make -j$(nproc)
	cd ..

clean:
	rm -rf build/

iso: $(KERNEL)
	@echo "Building Limine ISO..."
	@rm -rf $(ISO_DIR) $(OUT_DIR)/$(ISO)
	@mkdir -p $(ISO_DIR)/boot $(ISO_DIR)/bin $(OUT_DIR)
	@cp $(KERNEL) $(ISO_DIR)/boot/kernel.elf
	@cp limine.conf $(ISO_DIR)/
	@cp limine/limine-bios.sys $(ISO_DIR)/
	@cp limine/limine-bios-cd.bin $(ISO_DIR)/
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

# Run in QEMU
run: iso
	@echo "Booting ISO with QEMU..."
	qemu-system-x86_64 -m 1G -machine q35 \
	  -drive file=$(OUT_DIR)/$(ISO),if=ide,media=cdrom \
	  -boot order=d,once=d -serial stdio -no-reboot