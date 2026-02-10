#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
/* use our kmalloc/kfree */
#define STBI_MALLOC(x) kmalloc(x)
#define STBI_FREE(x) kfree(x)
/* Provide a realloc that can copy old data: */
#define STBI_REALLOC_SIZED(p, oldsz, newsz) stbi_realloc_sized(p, oldsz, newsz)
#define STBI_REALLOC(p, sz) stbi_realloc_sized(p, 0, sz)

#include "../mem/alloc.h"
#include <stddef.h>

/* Simple realloc implementation that uses kmalloc/kfree and copies the
   smaller of old/new sizes. This is sufficient for stb's use (expanding
   buffers during decoding). */
static void *stbi_realloc_sized(void *p, size_t oldsz, size_t newsz) {
  if (p == NULL)
    return kmalloc(newsz);
  void *n = kmalloc(newsz);
  if (!n) {
    kfree(p);
    return NULL;
  }
  size_t copy = oldsz < newsz ? oldsz : newsz;
  unsigned char *dn = (unsigned char *)n;
  unsigned char *dp = (unsigned char *)p;
  for (size_t i = 0; i < copy; ++i)
    dn[i] = dp[i];
  kfree(p);
  return n;
}

#include "stb_image.h"
