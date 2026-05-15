/*
 * 240p Test Suite - FM Towns port
 * Copyright (C) 2026 Artemio Urbina
 *
 * GPLv2 or later -- see LICENSE in repo root.
 *
 * --------------------------------------------------------------------------
 * FM Towns / Marty pad
 * --------------------------------------------------------------------------
 *
 * Port assignments (FM Towns Technical Data Book p.238, cross-checked
 * against Captain YS's IPL HID_IO.ASM):
 *
 *   0x04D0 (R) : PADA  -- pad in port 1 (the Marty's built-in pad)
 *   0x04D2 (R) : PADB  -- pad in port 2
 *   0x04D6 (W) : PAD_OUT strobe; MUST be written with 0x0F before reading
 *                (TRIG=1 / COM=0 -- enables D-pad + A/B in the same read,
 *                 MSX-pad-compatible mode)
 *
 * Bits in PADA after writing 0x0F to PAD_OUT
 * (FM TOWNS Technical Data Book Table I-7-9, all active-low: 0 = pressed):
 *
 *   bit 0 = FWD   (UP)
 *   bit 1 = BACK  (DOWN)
 *   bit 2 = LEFT
 *   bit 3 = RIGHT
 *   bit 4 = TRIG1 (A button)
 *   bit 5 = TRIG2 (B button)
 *   bit 6 = COM (1 = no input)
 *   bit 7 = (unused)
 *
 * SELECT and RUN are NOT separate bits. The FM Towns pad signals them
 * with impossible-D-pad chords (Table I-7-9):
 *
 *   LEFT  + RIGHT pressed at the same time  -> RUN
 *   UP    + DOWN  pressed at the same time  -> SELECT
 *
 * The decode below applies that rule: if we see L+R together we map it
 * to RUN and drop the L/R bits; same for U+D -> SELECT. This matches
 * the BIOS pattern in captainys/FM IPL HID_IO.ASM READ_PADA.
 *
 * The Marty has no keyboard, so the kbd path is a no-op there. On a
 * standard FM Towns we also drain keyboard scancodes for ESC and a few
 * navigation keys.
 */

#include "input.h"

#if defined(__FMTOWNS__)
#include "boot/io.h"
#else
static inline void outpb(u16 p, u8 v) { (void)p; (void)v; }
static inline u8   inpb (u16 p)       { (void)p; return 0x3F; }   /* "nothing pressed" stub */
static inline int  kbd_poll_scancode(void) { return -1; }
#endif

#define IO_PADA_IN  0x04D0
#define IO_PADB_IN  0x04D2
#define IO_PAD_OUT  0x04D6
#define PAD_OUT_VAL 0x0F           /* TRIG=1, COM=0 -- MSX-pad-compatible read */

#define IO_WAIT_1US 0x006C         /* OUT to this port = ~1us delay on >= 4th gen */

/* Bit positions in PADA / PADB after writing 0x0F to PAD_OUT
 * (book Table I-7-9). All active-low; this code has already inverted.
 */
#define BIT_UP      0x01   /* FWD  */
#define BIT_DOWN    0x02   /* BACK */
#define BIT_LEFT    0x04
#define BIT_RIGHT   0x08
#define BIT_A       0x10   /* TRIG1 */
#define BIT_B       0x20   /* TRIG2 */

static u16 g_held    = 0;
static u16 g_pressed = 0;
static u16 g_prev    = 0;

/* Forward declarations -- definitions live further down this file. */
static u16 read_pads(void);
static u16 read_keyboard(void);

void input_init(void)
{
    g_held = g_pressed = 0;
    /* Prime g_prev with whatever's currently on the pad/keyboard so the
     * first input_poll() doesn't fire a spurious press-edge for buttons
     * that were already held when the program started. This matters if
     * the pad's idle state on power-up reads any bit as "pressed"
     * (which we've seen on Tsugaru's Marty emulation).
     */
    g_prev = (u16)(read_pads() | read_keyboard());
}

/* Read one pad once, with the documented 1us settle wait + a simple
 * anti-chatter re-read when any button is held. Returns the 6-bit raw
 * value (bits 0..5) with active-low polarity already inverted, so a
 * set bit means "pressed".
 */
static u8 read_raw_pad(u16 port)
{
    u8 v;

    outpb(IO_PAD_OUT, PAD_OUT_VAL);
    outpb(IO_WAIT_1US, PAD_OUT_VAL);    /* TBIOS waits 1us here */

    v = (u8)(inpb(port) & 0x3F);
    if (v != 0x3F) {
        /* Something pressed -- give the line ~10ms to settle, re-read. */
        u16 i;
        for (i = 0; i < 10000; ++i)
            outpb(IO_WAIT_1US, PAD_OUT_VAL);
        v = (u8)(inpb(port) & 0x3F);
    }
    return (u8)(~v & 0x3F);             /* invert -> 1 means pressed */
}

static u16 decode_pad(u8 raw)
{
    u16 r = 0;

    /* RUN/SELECT chords (book Table I-7-9):
     *   LEFT  + RIGHT both pressed -> RUN
     *   UP    + DOWN  both pressed -> SELECT
     * Detect FIRST, then strip the offending direction bits so the
     * D-pad doesn't also fire.
     */
    if ((raw & (BIT_LEFT | BIT_RIGHT)) == (BIT_LEFT | BIT_RIGHT)) {
        r |= PAD_RUN;
        raw &= (u8)~(BIT_LEFT | BIT_RIGHT);
    }
    if ((raw & (BIT_UP | BIT_DOWN)) == (BIT_UP | BIT_DOWN)) {
        r |= PAD_SELECT;
        raw &= (u8)~(BIT_UP | BIT_DOWN);
    }

    if (raw & BIT_LEFT)  r |= PAD_LEFT;
    if (raw & BIT_RIGHT) r |= PAD_RIGHT;
    if (raw & BIT_UP)    r |= PAD_UP;
    if (raw & BIT_DOWN)  r |= PAD_DOWN;
    if (raw & BIT_A)     r |= PAD_A;
    if (raw & BIT_B)     r |= PAD_B;
    return r;
}

static u16 read_pads(void)
{
    /* OR pads A and B so either controller works (handy for testing on
     * a Towns with a second pad).
     */
    return (u16)(decode_pad(read_raw_pad(IO_PADA_IN)) |
                 decode_pad(read_raw_pad(IO_PADB_IN)));
}

/* Drain pending keyboard scancodes and translate to logical pad buttons.
 * The scancode values are the ones used by Captain YS's IPL menu code
 * (see HID_IO.ASM READ_PADA), which match the FM Towns BIOS make codes.
 */
static u16 read_keyboard(void)
{
    u16 r = 0;
    int sc;
    while ((sc = kbd_poll_scancode()) >= 0) {
        switch (sc & 0x7F) {
            case 0x4D: r |= PAD_UP;     break; /* arrow up    */
            case 0x50: r |= PAD_DOWN;   break; /* arrow down  */
            case 0x4F: r |= PAD_LEFT;   break; /* arrow left  */
            case 0x51: r |= PAD_RIGHT;  break; /* arrow right */
            case 0x35: r |= PAD_A;      break; /* Space -> A  */
            case 0x73: r |= PAD_RUN;    break; /* Execute     */
            case 0x1D: r |= PAD_RUN;    break; /* Return      */
            case 0x01: r |= PAD_QUIT;   break; /* ESC -> quit */
            /* "Cancel" / B button alternatives. ESC alone may be eaten
             * by the emulator's own UI, so several keys exit a pattern.
             */
            case 0x0F: r |= PAD_B;      break; /* Backspace -> B */
            case 0x2D: r |= PAD_B;      break; /* Z key    -> B */
            case 0x2E: r |= PAD_B;      break; /* X key    -> B */
            default: break;
        }
    }
    return r;
}

void input_poll(void)
{
    u16 cur = (u16)(read_pads() | read_keyboard());
    g_pressed = (u16)(cur & ~g_prev);
    g_held    = cur;
    g_prev    = cur;
}

u16 input_held   (void) { return g_held;    }
u16 input_pressed(void) { return g_pressed; }
