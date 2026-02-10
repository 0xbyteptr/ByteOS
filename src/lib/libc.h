#ifndef LIBC_H
#define LIBC_H

#include <stddef.h>
#include <stdarg.h>

/* Strings */
size_t strlen(const char *s);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
char *strcat(char *dst, const char *src);
char *strchr(const char *s, int c);

/* Memory */
void *memmove(void *dest, const void *src, size_t n);

/* Minimal allocation wrappers (maps to kmalloc/kfree) */
void *malloc(size_t size);
void free(void *ptr);

/* Simple conversions */
int atoi(const char *s);
long strtol(const char *nptr, char **endptr, int base);

/* Formatted I/O */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int snprintf(char *buf, size_t size, const char *fmt, ...);
int printf(const char *fmt, ...);

/* Additional utilities */
void *realloc(void *ptr, size_t size);
void *calloc(size_t nmemb, size_t size);
int memcmp(const void *s1, const void *s2, size_t n);
void *memchr(const void *s, int c, size_t n);
size_t strnlen(const char *s, size_t maxlen);
char *strrchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);
char *strdup(const char *s);
void bzero(void *s, size_t n);

#endif
