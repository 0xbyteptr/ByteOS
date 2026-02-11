#include "lib/libc.h"
#include "console/console.h"
#include "mem/alloc.h"
#include "serial/serial.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

size_t strlen(const char* s)
{
  const char* p = s;
  while (*p)
    ++p;
  return (size_t) (p - s);
}

char* strcpy(char* dst, const char* src)
{
  char* ret = dst;
  while ((*dst++ = *src++))
    ;
  return ret;
}

char* strncpy(char* dst, const char* src, size_t n)
{
  char*  d = dst;
  size_t i = 0;
  for (; i < n && src[i]; ++i)
    d[i] = src[i];
  for (; i < n; ++i)
    d[i] = '\0';
  return dst;
}

char* strcat(char* dst, const char* src)
{
  size_t off = strlen(dst);
  strcpy(dst + off, src);
  return dst;
}

char* strchr(const char* s, int c)
{
  while (*s)
  {
    if (*s == (char) c)
      return (char*) s;
    ++s;
  }
  return NULL;
}

void* memmove(void* dest, const void* src, size_t n)
{
  unsigned char*       d = dest;
  const unsigned char* s = src;
  if (d < s)
  {
    for (size_t i = 0; i < n; ++i)
      d[i] = s[i];
  }
  else
  {
    for (size_t i = n; i > 0; --i)
      d[i - 1] = s[i - 1];
  }
  return dest;
}

void* malloc(size_t size)
{
  return kmalloc(size);
}
void free(void* ptr)
{
  kfree(ptr);
}

void* realloc(void* ptr, size_t size)
{
  if (!ptr)
    return malloc(size);
  if (size == 0)
  {
    free(ptr);
    return NULL;
  }
  size_t old = kalloc_usable_size(ptr);
  void*  n   = malloc(size);
  if (!n)
    return NULL;
  /* copy the smaller of old and new sizes */
  size_t copy = old < size ? old : size;
  memmove(n, ptr, copy);
  free(ptr);
  return n;
}

void* calloc(size_t nmemb, size_t size)
{
  size_t tot = nmemb * size;
  void*  p   = malloc(tot);
  if (!p)
    return NULL;
  bzero(p, tot);
  return p;
}

int memcmp(const void* s1, const void* s2, size_t n)
{
  const unsigned char* a = s1;
  const unsigned char* b = s2;
  for (size_t i = 0; i < n; ++i)
  {
    if (a[i] < b[i])
      return -1;
    if (a[i] > b[i])
      return 1;
  }
  return 0;
}

void* memchr(const void* s, int c, size_t n)
{
  const unsigned char* p = s;
  for (size_t i = 0; i < n; ++i)
    if (p[i] == (unsigned char) c)
      return (void*) &p[i];
  return NULL;
}

size_t strnlen(const char* s, size_t maxlen)
{
  size_t i = 0;
  while (i < maxlen && s[i])
    ++i;
  return i;
}

char* strrchr(const char* s, int c)
{
  char* last = NULL;
  for (const char* p = s; *p; ++p)
    if (*p == (char) c)
      last = (char*) p;
  if (c == '\0')
    return (char*) (s + strlen(s));
  return last;
}

char* strstr(const char* haystack, const char* needle)
{
  if (!*needle)
    return (char*) haystack;
  for (; *haystack; ++haystack)
  {
    const char* h = haystack;
    const char* n = needle;
    while (*n && *h == *n)
    {
      ++h;
      ++n;
    }
    if (!*n)
      return (char*) haystack;
  }
  return NULL;
}

char* strdup(const char* s)
{
  size_t n = strlen(s) + 1;
  char*  p = malloc(n);
  if (!p)
    return NULL;
  memcpy(p, s, n);
  return p;
}

void bzero(void* s, size_t n)
{
  memset(s, 0, n);
}

int atoi(const char* s)
{
  long v = strtol(s, NULL, 10);
  return (int) v;
}

/* Minimal integer-to-string helper */
static char* u64_to_str(uint64_t v, char* buf_end, int base, int lowercase)
{
  static const char* digits_u = "0123456789ABCDEF";
  static const char* digits_l = "0123456789abcdef";
  const char*        digits   = lowercase ? digits_l : digits_u;
  char*              p        = buf_end;
  if (v == 0)
  {
    *--p = '0';
    return p;
  }
  while (v)
  {
    *--p = digits[v % base];
    v /= base;
  }
  return p;
}

int vsnprintf(char* buf, size_t size, const char* fmt, va_list ap)
{
  char*       out = buf;
  char*       end = buf + (size ? size - 1 : 0);
  const char* p   = fmt;
  while (*p)
  {
    if (*p != '%')
    {
      if (out < end)
        *out++ = *p++;
      else
        ++p;
      continue;
    }
    ++p; /* skip % */
    int longflag = 0;
    if (*p == 'l')
    {
      longflag = 1;
      ++p;
    }
    char spec = *p++;
    if (spec == 's')
    {
      const char* s = va_arg(ap, const char*);
      while (*s)
      {
        if (out < end)
          *out++ = *s++;
        else
          ++s;
      }
    }
    else if (spec == 'c')
    {
      char c = (char) va_arg(ap, int);
      if (out < end)
        *out++ = c;
    }
    else if (spec == 'd' || spec == 'i')
    {
      long v = longflag ? va_arg(ap, long) : va_arg(ap, int);
      if (v < 0)
      {
        if (out < end)
          *out++ = '-';
        v = -v;
      }
      char  tmp[32];
      char* s = u64_to_str((uint64_t) v, tmp + sizeof(tmp), 10, 0);
      while (*s)
      {
        if (out < end)
          *out++ = *s++;
        else
          ++s;
      }
    }
    else if (spec == 'u')
    {
      unsigned long v = longflag ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
      char          tmp[32];
      char*         s = u64_to_str((uint64_t) v, tmp + sizeof(tmp), 10, 0);
      while (*s)
      {
        if (out < end)
          *out++ = *s++;
        else
          ++s;
      }
    }
    else if (spec == 'x' || spec == 'X' || spec == 'p')
    {
      unsigned long v = va_arg(ap, unsigned long);
      char          tmp[32];
      char*         s = u64_to_str((uint64_t) v, tmp + sizeof(tmp), 16, spec == 'x');
      while (*s)
      {
        if (out < end)
          *out++ = *s++;
        else
          ++s;
      }
    }
    else if (spec == '%')
    {
      if (out < end)
        *out++ = '%';
    }
    else
    {
      /* Unknown spec, print it literally */
      if (out < end)
        *out++ = spec;
    }
  }
  if (size)
  {
    /* ensure final NUL fits inside buffer */
    if (out <= end)
      *out = '\0';
    else
      *(buf + size - 1) = '\0';
  }
  return (int) (out - buf);
}

int snprintf(char* buf, size_t size, const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int r = vsnprintf(buf, size, fmt, ap);
  va_end(ap);
  return r;
}

int printf(const char* fmt, ...)
{
  char    buf[512];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  buf[sizeof(buf) - 1] = '\0';
  /* print to framebuffer console and serial for logs */
  console_printf("%s", buf);
  return n;
}
