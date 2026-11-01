/* SPDX-License-Identifier: MIT */
/* Standalone diagnostic builds only; EDK II supplies its compiler intrinsics. */
#include <stddef.h>
void *memcpy(void *destination, const void *source, size_t n) {
  unsigned char *d=(unsigned char *)destination;
  const unsigned char *s=(const unsigned char *)source;
  size_t i;for(i=0;i<n;++i)d[i]=s[i];return destination;
}
void *memset(void *destination, int value, size_t n) {
  unsigned char *d=(unsigned char *)destination;
  size_t i;for(i=0;i<n;++i)d[i]=(unsigned char)value;return destination;
}
