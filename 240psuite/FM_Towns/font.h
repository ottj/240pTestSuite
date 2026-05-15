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
 */
void font_draw_text (VideoSurface *s, int x, int y, u8 fg, const char *text);

#endif
