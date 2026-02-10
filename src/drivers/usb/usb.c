#include "drivers/usb/usb.h"
#include "serial/serial.h"
#include <stddef.h>

static size_t dev_count = 0;

int usb_init(void) {
  serial_puts("usb: init - no host controllers available in this build\n");
  dev_count = 0;
  return 0;
}

size_t usb_get_device_count(void) { return dev_count; }
