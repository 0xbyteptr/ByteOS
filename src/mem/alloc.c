#include "alloc.h"
#include <stddef.h>
#include <stdint.h>

/* very small bump allocator with size header so we can implement realloc */
unsigned char heap[8 * 1024 * 1024] __attribute__((section(".bss"), aligned(16)));
static size_t heap_off = 0;

/* header stored immediately before the returned pointer */
struct kmalloc_hdr
{
  size_t size;
};

void* kmalloc(size_t size)
{
  /* simple align to 16 */
  size         = (size + 15) & ~15UL;
  size_t hdr   = (sizeof(struct kmalloc_hdr) + 15) & ~15UL;
  size_t total = hdr + size;
  if (heap_off + total > sizeof(heap))
    return NULL;
  struct kmalloc_hdr* h = (struct kmalloc_hdr*) &heap[heap_off];
  h->size               = size;
  void* r               = (void*) ((unsigned char*) h + hdr);
  heap_off += total;
  // Debug output: print heap address and offset
  extern void serial_puts(const char*);
  extern void serial_puthex64(uint64_t);
  serial_puts("kmalloc: heap base = 0x");
  serial_puthex64((uint64_t) (uintptr_t) heap);
  serial_puts(", heap_off = 0x");
  serial_puthex64((uint64_t) heap_off);
  serial_puts(", returned ptr = 0x");
  serial_puthex64((uint64_t) (uintptr_t) r);
  serial_puts("\n");
  return r;
}

void kfree(void* ptr)
{
  /* no-op for bump allocator */
  (void) ptr;
}

size_t kalloc_usable_size(void* ptr)
{
  if (!ptr)
    return 0;
  /* header is stored just before the returned pointer, aligned as above */
  size_t              hdr = (sizeof(struct kmalloc_hdr) + 15) & ~15UL;
  struct kmalloc_hdr* h   = (struct kmalloc_hdr*) ((unsigned char*) ptr - hdr);
  return h->size;
}
