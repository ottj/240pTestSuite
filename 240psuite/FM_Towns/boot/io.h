/*
 * Bare-metal I/O port + keyboard primitives for the FM Towns clang build.
 *
 * Replaces the speculative <towns/io.h> / <towns/kbd.h> we used as
 * placeholders while only host-compiling. Everything here is inline
 * assembly aimed at i386 in flat 32-bit mode.
 */

#ifndef FMT_BOOT_IO_H
#define FMT_BOOT_IO_H

#include <stdint.h>

/* ---- port I/O ----------------------------------------------------------- */

static inline void outpb(uint16_t port, uint8_t v)
{
    __asm__ __volatile__("outb %0, %1" :: "a"(v), "Nd"(port));
}

static inline void outpw(uint16_t port, uint16_t v)
{
    __asm__ __volatile__("outw %0, %1" :: "a"(v), "Nd"(port));
}

static inline uint8_t inpb(uint16_t port)
{
    uint8_t v;
    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline uint16_t inpw(uint16_t port)
{
    uint16_t v;
    __asm__ __volatile__("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

/* ---- keyboard ----------------------------------------------------------- *
 *
 * Drains one scancode from the FM Towns keyboard controller and returns it,
 * or -1 if none is pending. The interface is intentionally simple --
 * input.c calls this in a loop until it sees -1.
 *
 *   IO_KEYBOARD_STATE (0x0602):
 *     bit 0 = 1 -> a scancode is available in the data register
 *   IO_KEYBOARD_DATA  (0x0600):
 *     reading consumes one scancode byte
 *
 * Marty has no keyboard by default but accepts an optional one over the
 * same port; non-Marty machines have a keyboard wired up at boot. Either
 * way this poll is harmless when no key has been pressed.
 */

#define FMT_IO_KEYBOARD_DATA   0x0600
#define FMT_IO_KEYBOARD_STATE  0x0602

static inline int kbd_poll_scancode(void)
{
    if ((inpb(FMT_IO_KEYBOARD_STATE) & 0x01) == 0)
        return -1;
    return inpb(FMT_IO_KEYBOARD_DATA);
}

#endif
