#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/* Prevent aggressive auto-vectorization which emits SSE/XMM instructions
   that's unsafe in early kernel without enabling FPU/SSE state. Use scalar
   implementations and disable vectorizer for these functions. */
__attribute__((optimize("no-tree-vectorize", "no-unroll-loops"))) void *
memcpy(void *dest, const void *src, size_t n) {
  unsigned char *d = dest;
  const unsigned char *s = src;
  for (size_t i = 0; i < n; ++i)
    d[i] = s[i];
  return dest;
}

__attribute__((optimize("no-tree-vectorize", "no-unroll-loops"))) void *
memset(void *s, int c, size_t n) {
  unsigned char *p = s;
  unsigned char cc = (unsigned char)c;
  for (size_t i = 0; i < n; ++i)
    p[i] = cc;
  return s;
}

int strcmp(const char *a, const char *b) {
  while (*a && (*a == *b)) {
    ++a;
    ++b;
  }
  return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    if (a[i] != b[i] || a[i] == '\0' || b[i] == '\0')
      return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
  }
  return 0;
}

long strtol(const char *nptr, char **endptr, int base) {
  (void)endptr;
  (void)base;
  long sign = 1;
  long val = 0;
  while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n')
    ++nptr;
  if (*nptr == '+')
    ++nptr;
  else if (*nptr == '-') {
    sign = -1;
    ++nptr;
  }
  while (*nptr >= '0' && *nptr <= '9') {
    val = val * 10 + (*nptr - '0');
    ++nptr;
  }
  return sign * val;
}

/* alias used by some toolchains */
long __isoc23_strtol(const char *nptr, char **endptr, int base) {
  return strtol(nptr, endptr, base);
}

/* simple pow and ldexp for stb_image HDR paths; not highly accurate but
 * sufficient */
#include <math.h>

double pow(double a, double b) { // use a very small naive implementation
  double res = 1.0;
  int ib = (int)b;
  for (int i = 0; i < ib; ++i)
    res *= a;
  return res;
}

double ldexp(double x, int exp) {
  /* multiply by 2^exp */
  if (exp > 0)
    while (exp--)
      x *= 2.0;
  else
    while (exp++)
      x /= 2.0;
  return x;
}

int abs(int x) { return x < 0 ? -x : x; }

void __assert_fail(const char *assertion, const char *file, unsigned int line,
                   const char *function) {
  (void)assertion;
  (void)file;
  (void)line;
  (void)function;
  for (;;) {
    asm volatile("hlt");
  }
}
