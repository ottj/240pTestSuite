/*
 * 240p Test Suite - FM Towns port
 * Copyright (C) 2026 Artemio Urbina
 *
 * GPLv2 or later -- see LICENSE in repo root.
 */

#ifndef __FMT_FONT_H
#define __FMT_FONT_H

#include "types.h"
#include "video.h"

/* Tiny built-in 8x8 monochrome font. Only ASCII 0x20..0x7E are defined.
 * font_draw_text() writes pixels of palette index `fg` for set bits and
 * leaves background untouched.
 *
 * font_draw_text_scaled() does the same but replicates each set pixel
 * into a `scale x scale` block, so a `scale=2` glyph occupies 16x16
 * pixels and advances 16 per character. Use this on high-res
 * computer-monitor modes where 8x8 is unreadably small. scale must
 * be >= 1; scale=1 is identical to font_draw_text().
 */
void font_draw_text        (VideoSurface *s, int x, int y, u8 fg, const char *text);
void font_draw_text_scaled (VideoSurface *s, int x, int y, u8 fg, const char *text, int scale);

#endif
