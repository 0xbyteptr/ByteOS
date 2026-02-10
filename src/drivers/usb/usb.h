#ifndef USB_H
#define USB_H

#include <stddef.h>

int usb_init(void);
size_t usb_get_device_count(void);

#endif
