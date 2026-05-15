/*
 * 240p Test Suite - FM Towns port
 * Copyright (C) 2026 Artemio Urbina
 *
 * GPLv2 or later -- see LICENSE in repo root.
 */

#ifndef __FMT_VIDEO_H
#define __FMT_VIDEO_H

#include "types.h"

/*
 * FM Towns horizontal frequencies supported by the system.
 *
 *  15 kHz : ~15.7 kHz, NTSC-compatible TV output. Required for FM Towns Marty.
 *  24 kHz : ~24.8 kHz, the original FM Towns "computer monitor" mode.
 *  31 kHz : ~31.5 kHz, VGA / multisync monitor mode.
 *
 * For each frequency we expose one canonical 256-colour mode that fills
 * the active area. The patterns module renders into this surface.
 */
typedef enum {
    /* 256-colour modes */
    HFREQ_15KHZ_240P     = 0,  /* mode 11: 320x240 32768c, 15 kHz, 240p (Marty TV) */
    HFREQ_15KHZ_480I     = 1,  /* mode 14: 720x480 256c, 15 kHz interlace */
    HFREQ_24KHZ          = 2,  /* mode 13: 640x400 256c, 24 kHz */
    HFREQ_31KHZ          = 3,  /* mode 12: 640x480 256c, 31 kHz */

    /* 32768-colour ("high colour") modes -- what made the FM Towns famous */
    HFREQ_31KHZ_320x240  = 4,  /* mode 10: 320x240 32768c, 31 kHz */
    HFREQ_31KHZ_320x480  = 5,  /* mode 15: 320x480 32768c, 31 kHz */
    HFREQ_15KHZ_320x480  = 6,  /* mode 16: 320x480 32768c, 15 kHz interlace */
    HFREQ_31KHZ_512x480  = 7,  /* mode 17: 512x480 32768c, 31 kHz (flagship) */

    /* Custom (non-book) mode: 320x240 256c at 15 kHz non-interlace.
     * Derived from mode 11's 240p timing but with Layer 1 in 256c
     * single-screen instead of the 2-screen 32768c setup. */
    HFREQ_15KHZ_240P_256C = 8,

    /* Custom (non-book) mode: 320x240 32768c at 15 kHz non-interlace.
     * Derived from mode 11's 240p timing but flipped from 2-screen
     * 32768c (the book set 14 config that is broken in our current
     * implementation -- we only program one of the two layers) to
     * 1-screen 32768c on Layer 1. */
    HFREQ_15KHZ_240P_HC_1S = 9,

    HFREQ_COUNT
} HFreq;

/* Logical surface returned by video_get_surface(). The patterns module
 * draws into `pixels` as a packed 8-bit indexed bitmap. After drawing,
 * call video_flip() to make the changes visible (vsync wait).
 */
typedef struct {
    u8  *pixels;     /* points into VRAM                                    */
    u16  width;      /* logical pixels per line (NOT necessarily display
                      * pixels -- the 240p mode uses 4x H zoom)             */
    u16  height;     /* logical lines                                       */
    u16  pitch;      /* bytes per line in VRAM                              */
    u8   bpp;        /* 8 or 16: bits per pixel.
                      *   8  = palette index (256-colour modes)
                      *   16 = packed RGB-555, bit 15 transparent
                      *        (Table I-4-3, 32768-colour modes)            */
} VideoSurface;

/* Initialise the hardware once on startup.
 * Saves enough state to restore on exit, sets a default palette,
 * does not yet select a horizontal frequency -- call video_set_mode().
 */
void video_init(void);

/* Restore whatever video state existed before video_init(). */
void video_shutdown(void);

/* Switch the CRTC to the requested horizontal frequency.
 * Re-uploads the default palette and clears VRAM.
 * Returns 0 on success, non-zero if the mode could not be programmed.
 */
int  video_set_mode(HFreq f);

/* The currently active frequency (last successful video_set_mode). */
HFreq video_current_mode(void);

/* Human-readable name for a mode, e.g. "15 kHz / 320x240". */
const char *video_mode_name(HFreq f);

/* Get the current drawing surface. Valid until the next video_set_mode(). */
VideoSurface *video_get_surface(void);

/* Wait for vertical blank. */
void video_wait_vblank(void);

/* Set one palette entry (0..255). r/g/b are 0..255, the driver
 * converts to the hardware's 8-bit-per-channel palette.
 */
void video_set_palette(u8 index, u8 r, u8 g, u8 b);

/* Fill the visible surface with a single palette index. */
void video_clear(u8 index);

#endif
