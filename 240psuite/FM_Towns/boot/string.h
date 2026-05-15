/*
 * Minimal <string.h> shim for the freestanding clang i386-elf build.
 * Just declares the functions our kernel actually uses; their definitions
 * are in boot/kstd.c.
 *
 * On the host (macOS) the system <string.h> is picked up via the default
 * include path -- this shim only takes precedence when the kernel build
 * adds boot/ to the include search path with `-isystem boot` (so it wins
 * the angle-bracket `#include <string.h>` lookup).
 */

#ifndef FMT_BOOT_STRING_H
#define FMT_BOOT_STRING_H

#include <stddef.h>

void  *memset (void *dst, int c, size_t n);
void  *memcpy (void *dst, const void *src, size_t n);
void  *memmove(void *dst, const void *src, size_t n);
int    memcmp (const void *a, const void *b, size_t n);
size_t strlen (const char *s);

#endif
