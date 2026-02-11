#include "limine.h"

/* Definitions (single instance) placed in the special section the bootloader
   searches for. */
__attribute__((used, section(".limine_reqs.base"))) volatile uint64_t limine_base_revision[3] = {
    LIMINE_COMMON_MAGIC_0, LIMINE_COMMON_MAGIC_1, 0};

__attribute__((used, section(".limine_reqs.requests"))) volatile struct limine_framebuffer_request
    framebuffer_request = {.id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0, .response = NULL};
