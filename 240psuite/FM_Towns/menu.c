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
#include <string.h>

enum {
    MENU_BARS = 0,
    MENU_GRID,
    MENU_MONOSCOPE,
    MENU_SOLIDS,
    MENU_RAINBOW,
    /* 256-colour modes, by resolution starting at 240p, then by H freq. */
    MENU_RES_15_240P_256C,         /* custom (non-book) */
    MENU_RES_15_480I,              /* M14 */
    MENU_RES_24,                   /* M13 */
    MENU_RES_31,                   /* M12 */
    /* 32768-colour ("high colour") modes, same secondary ordering.
     * Custom variants appear before their book-mode counterparts so
     * working modes show up first in the menu. */
    MENU_RES_15_240P_HC_1S,        /* custom 1-screen 240p HC */
    MENU_RES_15_240P,              /* M11 (book set 14, currently broken) */
    MENU_RES_31_320x240_HC_1S,     /* custom 1-screen 31 kHz 320x240 HC */
    MENU_RES_31_320x240_HC,        /* M10 (book, currently broken) */
    MENU_RES_15_320x480_HC,        /* M16 */
    MENU_RES_31_320x480_HC,        /* M15 */
    MENU_RES_31_512x480_HC,        /* M17 */
    MENU_QUIT,
    MENU_COUNT
};

/* Labels are kept to 18 chars max so the whole menu fits in 240p mode's
 * BLOCK_W width (~144 px at the 1x font). "HC" = High Colour = 32768
 * colours. "256C" = 256 colours. "*" originally marked modes that
 * worked cleanly on Tsugaru-Marty emulation -- after the 2026-05-15
 * real-hardware pass the semantics are a bit muddled; see the
 * "Real-Marty test results" table in README.md for the up-to-date
 * status. "CST" = custom (non-book) CRTC config, untested.
 */
static const char *const labels[MENU_COUNT] = {
    "COLOR BARS",
    "GRID",
    "MONOSCOPE",
    "SOLID COLORS",
    "RAINBOW",
    "CST 15K 240P 256C",
    "M14 15K 480I 256C",
    "M13 24K 640X400 *",
    "M12 31K 640X480 *",
    "CST 15K 240P  HC ",   /* 1-screen variant of mode 11 */
    "M11 15K 240P  HC ",
    "CST 31K 320X240HC",   /* 1-screen variant of mode 10 */
    "M10 31K 320X240HC",
    "M16 15K 320X480HC",
    "M15 31K 320X480HC",
    "M17 31K 512X480 *",
    "QUIT"
};

/* Base (1x) row height / cursor column / menu block width. The actual
 * pixel dimensions used while drawing are these multiplied by the
 * mode-dependent scale (menu_scale_for_mode).
 *
 * Widest label is 18 chars; BLOCK_W_BASE accounts for the cursor
 * "> " in front. At 1x: 160 logical pixels; at 2x: 320.
 */
#define ROW_H_BASE      8
#define CURSOR_W_BASE   12
#define BLOCK_W_BASE    ((18 + 2) * 8)

/* Pick the font scale per mode. Goal: the menu looks similar in apparent
 * size on whatever display the mode actually drives.
 *
 *  - 15 kHz TV modes (M11 240p, M14 480i) -> 1x. On a CRT TV the 8x8
 *    font is comfortably readable; this matches real-hardware testing
 *    on M14.
 *  - 24/31 kHz progressive modes at 640+ width (M12, M13, M17) -> 2x.
 *    These drive a computer monitor where 8x8 is unreadably small.
 *  - 320-wide modes (M10, M15, M16) -> 1x. BLOCK_W_BASE already takes
 *    half the width at 1x, so 2x would overflow horizontally.
 */
static int menu_scale_for_mode(HFreq m)
{
    switch (m) {
        case HFREQ_24KHZ:
        case HFREQ_31KHZ:
        case HFREQ_31KHZ_512x480:
            return 2;
        default:
            return 1;
    }
}

/* Centre a text horizontally on its own pixel width. */
static int centre_x(int screen_w, const char *t, int scale)
{
    int w = (int)strlen(t) * 8 * scale;
    int x = (screen_w - w) / 2;
    return (x < 0) ? 0 : x;
}

static void draw_menu(int sel)
{
    VideoSurface *s = video_get_surface();
    int scale       = menu_scale_for_mode(video_current_mode());
    int row_h_menu  = ROW_H_BASE * scale;          /* menu items scale */
    int row_h_info  = ROW_H_BASE;                  /* info bar always 1x */
    int cursor_w    = CURSOR_W_BASE * scale;
    int block_w     = BLOCK_W_BASE  * scale;
    int items_h     = MENU_COUNT * row_h_menu;
    int info_h      = 2 * row_h_info;              /* title row + mode-name row */
    int gap         = row_h_info;                  /* gap between info and items */
    int total_h     = info_h + gap + items_h;
    int items_top   = (s->height - total_h) / 2 + info_h + gap;
    int item_cx     = (s->width - block_w) / 2;
    int title_y, mode_y;
    int i;

    if (items_top < info_h + gap) items_top = info_h + gap;
    if (item_cx   < 0)            item_cx   = 0;
    title_y = items_top - info_h - gap;
    mode_y  = items_top - row_h_info - gap;

    /* palette: 0 = black, 1 = white, 2 = highlight (yellow) */
    video_set_palette(0,   0,   0,   0);
    video_set_palette(1, 192, 192, 192);
    video_set_palette(2, 255, 255,   0);
    video_clear(0);

    /* Info bar: always 1x scale, horizontally centred on its own text
     * width. Long mode-name strings (~33 chars) overflow if drawn at
     * the menu's scale, especially at scale 2 / on 320-wide surfaces. */
    {
        const char *title = "240P TEST SUITE";
        font_draw_text(s, centre_x(s->width, title, 1), title_y, 1, title);
    }
    {
        char buf[40];
        const char *m = video_mode_name(video_current_mode());
        int j;
        for (j = 0; j < 39 && m[j]; ++j) buf[j] = m[j];
        buf[j] = 0;
        font_draw_text(s, centre_x(s->width, buf, 1), mode_y, 1, buf);
    }

    for (i = 0; i < MENU_COUNT; ++i) {
        u8 col = (i == sel) ? 2 : 1;
        font_draw_text_scaled(s, item_cx + cursor_w, items_top + i * row_h_menu, col,
                              labels[i], scale);
        if (i == sel)
            font_draw_text_scaled(s, item_cx, items_top + i * row_h_menu, 2, ">", scale);
    }
}

static void run_item(int item)
{
    switch (item) {
        case MENU_BARS:                pattern_color_bars();                  break;
        case MENU_GRID:                pattern_grid();                        break;
        case MENU_MONOSCOPE:           pattern_monoscope();                   break;
        case MENU_SOLIDS:              pattern_solid_colors();                break;
        case MENU_RAINBOW:             pattern_rainbow();                     break;
        case MENU_RES_15_240P_256C:    video_set_mode(HFREQ_15KHZ_240P_256C); break;
        case MENU_RES_15_480I:         video_set_mode(HFREQ_15KHZ_480I);      break;
        case MENU_RES_24:              video_set_mode(HFREQ_24KHZ);           break;
        case MENU_RES_31:              video_set_mode(HFREQ_31KHZ);           break;
        case MENU_RES_15_240P_HC_1S:   video_set_mode(HFREQ_15KHZ_240P_HC_1S);   break;
        case MENU_RES_15_240P:         video_set_mode(HFREQ_15KHZ_240P);         break;
        case MENU_RES_31_320x240_HC_1S:video_set_mode(HFREQ_31KHZ_320x240_HC_1S);break;
        case MENU_RES_31_320x240_HC:   video_set_mode(HFREQ_31KHZ_320x240);      break;
        case MENU_RES_15_320x480_HC:   video_set_mode(HFREQ_15KHZ_320x480);   break;
        case MENU_RES_31_320x480_HC:   video_set_mode(HFREQ_31KHZ_320x480);   break;
        case MENU_RES_31_512x480_HC:   video_set_mode(HFREQ_31KHZ_512x480);   break;
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
