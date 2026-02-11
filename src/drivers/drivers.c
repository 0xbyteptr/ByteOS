#include "drivers/drivers.h"
#include "serial/serial.h"

/* Forward declarations for driver init functions (stubs implemented per-driver)
 */
int ata_init(void);
int usb_init(void);
int keyboard_init(void);
int mouse_init(void);
int fat_init(void);
int ext_init(void);

void drivers_init_all(void)
{
  serial_puts("drivers: init all\n");
  if (ata_init() == 0)
    serial_puts("drivers: ata ok\n");
  if (usb_init() == 0)
    serial_puts("drivers: usb ok\n");
  if (keyboard_init() == 0)
    serial_puts("drivers: keyboard ok\n");
  if (mouse_init() == 0)
    serial_puts("drivers: mouse ok\n");
  if (fat_init() == 0)
    serial_puts("drivers: fat ok\n");
  if (ext_init() == 0)
    serial_puts("drivers: ext ok\n");
}
