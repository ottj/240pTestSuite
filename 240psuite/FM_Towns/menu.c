/*
 * 240p Test Suite - FM Towns port
 * Copyright (C) 2026 Artemio Urbina
 *
 * GPLv2 or later -- see LICENSE in repo root.
 */

#include "menu.h"
#include "video.h"
#include "input.h"
#include "font.h"
#include "patterns.h"
#include "types.h"

enum {
    MENU_BARS = 0,
    MENU_GRID,
    MENU_MONOSCOPE,
    MENU_SOLIDS,
    MENU_RES_15_240P,
    MENU_RES_15_480I,
    MENU_RES_24,
    MENU_RES_31,
    MENU_RES_31_320x240_HC,
    MENU_RES_31_320x480_HC,
    MENU_RES_15_320x480_HC,
    MENU_RES_31_512x480_HC,
    MENU_QUIT,
    MENU_COUNT
};

/*
 * Labels are kept short so the whole menu fits in 240p mode's
 * visible area (about 288 pixels wide / 220 lines after overscan).
 * Longest label is 18 chars = 144 pixels at the 8x8 font.
 */
/* Labels kept to 18 chars so they fit in the menu BLOCK_W width.
 * "HC" = High Colour = 32768 colours. "256C" = 256 colours.
 * "*" marks modes that work cleanly on Tsugaru-Marty.
 * Modes without "*" render with various Tsugaru emulation quirks
 * (line stride, interlace, 2-screen compositing); they may render
 * correctly on real Marty hardware.
 */
static const char *const labels[MENU_COUNT] = {
    "COLOR BARS",
    "GRID",
    "MONOSCOPE",
    "SOLID COLORS",
    "M11 15K 240P  HC ",
    "M14 15K 480I 256C",
    "M13 24K 640X400 *",
    "M12 31K 640X480 *",
    "M10 31K 320X240HC",
    "M15 31K 320X480HC",
    "M16 15K 320X480HC",
    "M17 31K 512X480 *",
    "QUIT"
};

/* Row height = font height; no extra inter-row spacing. Lets the whole
 * menu fit in 240p's visible area (Tsugaru displays only the centre
 * portion of mode 11's 240-line surface).
 */
#define ROW_H        8
#define CURSOR_W     12
/* Widest label is 18 chars; account for the cursor "> " in front. */
#define BLOCK_W      ((18 + 2) * 8)

static void draw_menu(int sel)
{
    VideoSurface *s = video_get_surface();
    int i;
    int items_h = MENU_COUNT * ROW_H + 2 * ROW_H;   /* +title +mode */
    int top     = (s->height - items_h) / 2 + 2 * ROW_H;
    int cx      = (s->width  - BLOCK_W) / 2;
    if (top < ROW_H) top = ROW_H;
    if (cx  < 0)     cx  = 0;

    /* palette: 0 = black, 1 = white, 2 = highlight (yellow) */
    video_set_palette(0,   0,   0,   0);
    video_set_palette(1, 192, 192, 192);
    video_set_palette(2, 255, 255,   0);
    video_clear(0);

    font_draw_text(s, cx, top - 2 * ROW_H, 1, "240P TEST SUITE");
    {
        char buf[40];
        const char *m = video_mode_name(video_current_mode());
        int j;
        for (j = 0; j < 39 && m[j]; ++j) buf[j] = m[j];
        buf[j] = 0;
        font_draw_text(s, cx, top - ROW_H, 1, buf);
    }

    for (i = 0; i < MENU_COUNT; ++i) {
        u8 col = (i == sel) ? 2 : 1;
        font_draw_text(s, cx + CURSOR_W, top + i * ROW_H, col, labels[i]);
        if (i == sel)
            font_draw_text(s, cx, top + i * ROW_H, 2, ">");
    }
}

static void run_item(int item)
{
    switch (item) {
        case MENU_BARS:                pattern_color_bars();            break;
        case MENU_GRID:                pattern_grid();                  break;
        case MENU_MONOSCOPE:           pattern_monoscope();             break;
        case MENU_SOLIDS:              pattern_solid_colors();          break;
        case MENU_RES_15_240P:         video_set_mode(HFREQ_15KHZ_240P);    break;
        case MENU_RES_15_480I:         video_set_mode(HFREQ_15KHZ_480I);    break;
        case MENU_RES_24:              video_set_mode(HFREQ_24KHZ);         break;
        case MENU_RES_31:              video_set_mode(HFREQ_31KHZ);         break;
        case MENU_RES_31_320x240_HC:   video_set_mode(HFREQ_31KHZ_320x240); break;
        case MENU_RES_31_320x480_HC:   video_set_mode(HFREQ_31KHZ_320x480); break;
        case MENU_RES_15_320x480_HC:   video_set_mode(HFREQ_15KHZ_320x480); break;
        case MENU_RES_31_512x480_HC:   video_set_mode(HFREQ_31KHZ_512x480); break;
        default: break;
    }
}

void menu_run(void)
{
    int sel = 0;
    int redraw = 1;

    for (;;) {
        u16 p;
        if (redraw) { draw_menu(sel); redraw = 0; }

        video_wait_vblank();
        input_poll();
        p = input_pressed();

        if (p & PAD_UP)     { sel = (sel - 1 + MENU_COUNT) % MENU_COUNT; redraw = 1; }
        if (p & PAD_DOWN)   { sel = (sel + 1)             % MENU_COUNT; redraw = 1; }
        if (p & PAD_CONFIRM) {
            if (sel == MENU_QUIT) return;
            run_item(sel);
            redraw = 1;
        }
        if (p & PAD_QUIT)   return;
    }
}
