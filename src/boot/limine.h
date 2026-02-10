#ifndef LIMINE_H
#define LIMINE_H
#include <stddef.h>
#include <stdint.h>

/* LIMINE COMMON MAGIC (first two 64-bit words) */
#define LIMINE_COMMON_MAGIC_0 0xc7b1dd30df4c8b88ULL
#define LIMINE_COMMON_MAGIC_1 0x0a82e883a194f07bULL

/* Framebuffer request ID: {LIMINE_COMMON_MAGIC, 0x9d5827dcd881dd75, 0xa3148604f6fab11b} */
#define LIMINE_FRAMEBUFFER_REQUEST_ID_2 0x9d5827dcd881dd75ULL
#define LIMINE_FRAMEBUFFER_REQUEST_ID_3 0xa3148604f6fab11bULL

#define LIMINE_FRAMEBUFFER_REQUEST \
    {LIMINE_COMMON_MAGIC_0, LIMINE_COMMON_MAGIC_1, LIMINE_FRAMEBUFFER_REQUEST_ID_2, LIMINE_FRAMEBUFFER_REQUEST_ID_3}

/* Basic structures copied/adapted from Limine PROTOCOL.md (minimal subset) */
struct limine_framebuffer;
struct limine_framebuffer_response {
    uint64_t revision;
    uint64_t framebuffer_count;
    struct limine_framebuffer **framebuffers;
};

struct limine_framebuffer_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_framebuffer_response *response;
};

/* memory_model constants */
#define LIMINE_FRAMEBUFFER_RGB 1

struct limine_framebuffer {
    void *address; /* HHDM address (kernel virtual) */
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp; /* bits per pixel */
    uint8_t memory_model; /* e.g. LIMINE_FRAMEBUFFER_RGB */
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
    uint8_t unused[7];
    uint64_t edid_size;
    void *edid;
    /* revision 1 fields follow, but we don't need them for now */
};

/* Request storage is defined in a single C file to ensure the bootloader
   populates a single instance. Declarations here are extern to avoid
   multiple-definition/linkage issues when this header is included by
   multiple translation units. */
extern volatile uint64_t limine_base_revision[3];
extern volatile struct limine_framebuffer_request framebuffer_request;

#endif /* LIMINE_H */
