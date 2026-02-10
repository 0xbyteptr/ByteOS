#ifndef CONSOLE_H
#define CONSOLE_H

#include <stddef.h>

void console_init(void);
void console_puts(const char *s);
void console_printf(const char *fmt, ...);

#endif
