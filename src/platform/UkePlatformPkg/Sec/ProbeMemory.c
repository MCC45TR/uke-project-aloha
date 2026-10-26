/* SPDX-License-Identifier: MIT */
/* Freestanding libfdt read-only dependencies; no allocator or runtime. */
#include <stddef.h>
size_t strlen(const char *s) { size_t n = 0; while (s[n]) ++n; return n; }
size_t strnlen(const char *s, size_t limit) {
  size_t n = 0; while (n < limit && s[n]) ++n; return n;
}
int memcmp(const void *a, const void *b, size_t n) {
  const unsigned char *x = a, *y = b;
  for (size_t i = 0; i < n; ++i) if (x[i] != y[i]) return x[i] - y[i];
  return 0;
}
void *memchr(const void *s, int value, size_t n) {
  const unsigned char *p = s;
  for (size_t i = 0; i < n; ++i) if (p[i] == (unsigned char)value) return (void *)(p + i);
  return 0;
}
char *strrchr(const char *s, int value) {
  const char *found = 0;
  do { if (*s == (char)value) found = s; } while (*s++);
  return (char *)found;
}
