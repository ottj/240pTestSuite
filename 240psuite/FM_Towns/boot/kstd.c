/*
 * Minimal libc-replacements for the freestanding clang -target i386-elf
 * build. Clang emits implicit calls to memset/memcpy/memmove for struct
 * assignments and array initializers; provide them so linking succeeds.
 *
 * These are deliberately simple. The kernel does not need fast string
 * routines -- the only big copy is the framebuffer paint, and that's
 * done with explicit inline asm or open-coded loops in the patterns
 * module.
 */

#include <stdint.h>
#include <stddef.h>

/* Bulk writes/copies through 32-bit aligned chunks. The 386SX has a
 * 16-bit bus so 32-bit writes still take two cycles, but they save
 * the per-byte loop overhead -- decisive for VRAM clears.
 */

void *memset(void *dst, int c, size_t n)
{
    unsigned char *d = (unsigned char *)dst;

    while (((unsigned int)(uintptr_t)d & 3) && n) {
        *d++ = (unsigned char)c;
        --n;
    }
    if (n >= 4) {
        unsigned int  c4 = (unsigned int)(unsigned char)c;
        unsigned int *p4;
        size_t        n4;
        c4 |= c4 << 8;
        c4 |= c4 << 16;
        p4 = (unsigned int *)d;
        n4 = n >> 2;
        n -= n4 << 2;
        while (n4--) *p4++ = c4;
        d = (unsigned char *)p4;
    }
    while (n--) *d++ = (unsigned char)c;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    if ((((unsigned int)(uintptr_t)d | (unsigned int)(uintptr_t)s) & 3) == 0) {
        unsigned int       *p4 = (unsigned int *)d;
        const unsigned int *q4 = (const unsigned int *)s;
        size_t              n4 = n >> 2;
        n -= n4 << 2;
        while (n4--) *p4++ = *q4++;
        d = (unsigned char *)p4;
        s = (const unsigned char *)q4;
    }
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d == s || n == 0) return dst;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *p = (const unsigned char *)a;
    const unsigned char *q = (const unsigned char *)b;
    while (n--) {
        if (*p != *q) return (int)*p - (int)*q;
        ++p; ++q;
    }
    return 0;
}

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p) ++p;
    return (size_t)(p - s);
}
