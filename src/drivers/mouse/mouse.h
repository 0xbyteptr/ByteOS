#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

int mouse_init(void);
int mouse_get_packet(uint8_t out[3]);

#endif
