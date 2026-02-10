#include "alloc.h"
#include <stddef.h>
#include <stdint.h>

/* very small bump allocator with size header so we can implement realloc */
static unsigned char heap[1024 * 1024]; /* 1 MiB */
static size_t heap_off = 0;

/* header stored immediately before the returned pointer */
struct kmalloc_hdr {
  size_t size;
};

void *kmalloc(size_t size) {
  /* simple align to 16 */
  size = (size + 15) & ~15UL;
  size_t hdr = (sizeof(struct kmalloc_hdr) + 15) & ~15UL;
  size_t total = hdr + size;
  if (heap_off + total > sizeof(heap))
    return NULL;
  struct kmalloc_hdr *h = (struct kmalloc_hdr *)&heap[heap_off];
  h->size = size;
  void *r = (void *)((unsigned char *)h + hdr);
  heap_off += total;
  return r;
}

void kfree(void *ptr) {
  /* no-op for bump allocator */
  (void)ptr;
}

size_t kalloc_usable_size(void *ptr) {
  if (!ptr)
    return 0;
  /* header is stored just before the returned pointer, aligned as above */
  size_t hdr = (sizeof(struct kmalloc_hdr) + 15) & ~15UL;
  struct kmalloc_hdr *h = (struct kmalloc_hdr *)((unsigned char *)ptr - hdr);
  return h->size;
}
