ByteOS — framebuffer splash & console background (Limine)

What I added ✅
- A minimal Limine-compatible request header in `src/boot/limine.h` (framebuffer request)
- Tiny kernel entry in `src/boot/entry.S`
- Kernel that decodes `assets/bg.jpg` and blits it to the framebuffer: `src/kernel/main.c`
- Small bump allocator `src/mem/alloc.c`
- `stb_image` integration (downloaded at build time if missing) and a wrapper in `src/lib/stb_image_impl.c`
- Convenience Makefile for building and running with QEMU (fallback when Limine isn't installed)
- `limine.cfg` (minimal) and `linker.ld`
- `src/assets/bg.c` generation step (created by `make` from `assets/bg.jpg`)

How to build & test 🔧
1. Build:
   make
   - This will fetch `stb_image.h` into `src/lib/stb_image_stub.h` if missing and generate `src/assets/bg.c` from `assets/bg.jpg`.

2. Run (quick test):
   make run
   - If `limine-install` is not available it will boot directly with `qemu -kernel kernel.elf` (no limine). This still shows the framebuffer splash (the kernel blits it early).

3. To boot with the Limine bootloader properly:
   - Use `make iso` to produce `byteos.iso` (this requires a `limine/` folder with a Limine release).
   - If QEMU doesn't boot the ISO with `make run`, try the helper script that forces CD boot:
     ./run-iso.sh
   - Alternatively run:
     qemu-system-x86_64 -m 1G -machine q35 -drive file=byteos.iso,if=ide,media=cdrom -boot order=d,once=d -serial stdio
   - Install limine files (`limine.sys`, `limine-*.bin`) into the build folder and create a bootable drive using `limine-install` (see Limine docs). The `Makefile` has a placeholder target `iso` with a hint; if you want I can add a full limine iso installer target.

Next steps I can do (pick which you want) 💡
1) Add a proper PSF font and real text console overlay (glyph rendering). ✅
2) Create a full Limine ISO installer target in `Makefile` that produces a bootable image with Limine and boots via QEMU. ✅
3) Add scaling/letterbox support and simple alpha blending for console. ✅

Tell me which next step you'd like me to implement and I will proceed.
