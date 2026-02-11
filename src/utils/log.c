#include "lib/libc.h"
#include "serial/serial.h"

void log(const char* msg)
{
  serial_puts("[LOG] ");
  serial_puts(msg);
  serial_puts("\n");
  // Check if msg is an integer string and print as integer if so
  int         is_int = 1;
  const char* p      = msg;
  if (*p == '-' || *p == '+')
    p++;
  if (*p == '\0')
    is_int = 0;
  while (*p)
  {
    if (*p < '0' || *p > '9')
    {
      is_int = 0;
      break;
    }
    p++;
  }
  if (is_int)
  {
    int val = atoi(msg);
    printf("[LOG] %d\n", val);
    serial_puts("[LOG] ");
    serial_putdec(val);
    serial_puts("\n");
  }
  else
  {
    printf("[LOG] %s\n", msg);
  }
}