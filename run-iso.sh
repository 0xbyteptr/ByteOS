#!/usr/bin/env bash
set -eu
ISO=byteos.iso
if [ ! -f "$ISO" ]; then
  echo "ISO '$ISO' not found. Run 'make iso' first." >&2
  exit 1
fi

echo "Starting QEMU (explicit CD drive, Q35 machine) - this forces boot from the CD"
qemu-system-x86_64 \
  -m 1G \
  -machine q35 \
  -drive file=$ISO,if=ide,media=cdrom \
  -boot order=d,once=d \
  -serial stdio \
  -no-reboot
