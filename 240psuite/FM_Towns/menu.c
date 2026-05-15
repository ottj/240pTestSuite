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
     * The book modes M11 (2-screen set 14) and M10 (2-screen, 320-dot
     * active area) don't render correctly in our code and were dropped
     * from the menu; the CST variants take their place. The CRTC tables
     * for the original M11/M10 stay in video.c as book references for
     * anyone wanting to investigate the 2-screen layer setup. */
    MENU_RES_15_240P_HC_1S,        /* custom 1-screen 240p HC (was M11) */
    MENU_RES_31_320x240_HC_1S,     /* custom 1-screen 31 kHz 320x240 HC (was M10) */
    MENU_RES_15_320x480_HC,        /* M16 */
    MENU_RES_31_320x480_HC,        /* M15 */
    MENU_RES_31_512x480_HC,        /* M17 */
    MENU_QUIT,
    MENU_COUNT
};

/* Labels follow a single uniform 20-char layout for the mode entries:
 *
 *     MMM FFF RRRRRRR DDDD
 *     ^^^ ^^^ ^^^^^^^ ^^^^
 *      |   |     |     +-- colour depth: "256C" or "HC" (right-padded)
 *      |   |     +-- resolution, right-padded to 7 chars
 *      |   +-- horizontal frequency: "15K" / "24K" / "31K"
 *      +-- mode tag: "M11".."M17" for book modes, "CST" for custom
 *
 * Pattern entries ("COLOR BARS", "RAINBOW", "QUIT") don't follow this
 * format -- they're free-form left-aligned.
 */
static const char *const labels[MENU_COUNT] = {
    "COLOR BARS",
    "GRID",
    "MONOSCOPE",
    "SOLID COLORS",
    "RAINBOW",
    "CST 15K 240P    256C",
    "M14 15K 480I    256C",
    "M13 24K 640X400 256C",
    "M12 31K 640X480 256C",
    "CST 15K 240P    HC  ",
    "CST 31K 320X240 HC  ",
    "M16 15K 320X480 HC  ",
    "M15 31K 320X480 HC  ",
    "M17 31K 512X480 HC  ",
    "QUIT"
};

/* Base (1x) row height / cursor column / menu block width. The actual
 * pixel dimensions used while drawing are these multiplied by the
 * mode-dependent scale (menu_scale_for_mode).
 *
 * Widest label is 20 chars (the normalised mode-entry format); plus 2
 * chars budget for the "> " cursor. At 1x: 176 logical pixels.
 */
#define ROW_H_BASE      8
#define CURSOR_W_BASE   12
#define BLOCK_W_BASE    ((20 + 2) * 8)

/* Pick the font scale per mode. Goal: the menu looks similar in apparent
 * size on whatever display the mode actually drives.
 *
 *  - 15 kHz TV modes (240p, 480i) -> 1x. On a CRT TV the 8x8 font is
 *    comfortably readable.
 *  - 24/31 kHz progressive modes at 640+ width (M12, M13, M17) -> 2x.
 *    These drive a computer monitor where 8x8 is unreadably small.
 *  - 320-wide modes (CST 31K, M15, M16) -> 1x. The 20-char menu block
 *    is 176 px at 1x; 2x would overflow on a 320-wide surface.
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
    int row_h_menu  = ROW_H_BASE * scale;          /* menu items scale  */
    int row_h_info  = ROW_H_BASE;                  /* info bar always 1x */
    int cursor_w    = CURSOR_W_BASE * scale;
    int block_w     = BLOCK_W_BASE  * scale;
    int items_h     = MENU_COUNT * row_h_menu;
    int info_h      = 2 * row_h_info;              /* title + mode-name rows */
    int footer_h    = row_h_info;                  /* single-line footer    */
    int gap         = row_h_info;
    int total_h     = info_h + gap + items_h + gap + footer_h;
    int items_top   = (s->height - total_h) / 2 + info_h + gap;
    int item_cx     = (s->width - block_w) / 2;
    int title_y, mode_y, footer_y;
    int i;

    if (items_top < info_h + gap) items_top = info_h + gap;
    if (item_cx   < 0)            item_cx   = 0;
    title_y  = items_top - info_h - gap;
    mode_y   = items_top - row_h_info - gap;
    footer_y = items_top + items_h + gap;

    /* palette: 0 = black, 1 = white, 2 = highlight (yellow) */
    video_set_palette(0,   0,   0,   0);
    video_set_palette(1, 192, 192, 192);
    video_set_palette(2, 255, 255,   0);
    video_clear(0);

    /* Info bar (top): always 1x scale, centred on its own text width.
     * Long mode-name strings would overflow if drawn at the menu's
     * scale on a narrow surface. */
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

    /* Menu items at the per-mode scale. */
    for (i = 0; i < MENU_COUNT; ++i) {
        u8 col = (i == sel) ? 2 : 1;
        font_draw_text_scaled(s, item_cx + cursor_w, items_top + i * row_h_menu, col,
                              labels[i], scale);
        if (i == sel)
            font_draw_text_scaled(s, item_cx, items_top + i * row_h_menu, 2, ">", scale);
    }

    /* Footer (bottom): controls cheat sheet at 1x. Equals-sign style
     * keeps it under 35 chars so it fits centred in a 320 px wide
     * surface (worst case is the HC modes at 320 width). L/R + B are
     * only meaningful inside a pattern but listed here once so the
     * user doesn't have to remember. */
    {
        const char *help = "UP/DN=MOVE  A=SEL  L/R=MODE  B=BACK";
        font_draw_text(s, centre_x(s->width, help, 1), footer_y, 1, help);
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
        case MENU_RES_31_320x240_HC_1S:video_set_mode(HFREQ_31KHZ_320x240_HC_1S);break;
        case MENU_RES_15_320x480_HC:   video_set_mode(HFREQ_15KHZ_320x480);      break;
        case MENU_RES_31_320x480_HC:   video_set_mode(HFREQ_31KHZ_320x480);      break;
        case MENU_RES_31_512x480_HC:   video_set_mode(HFREQ_31KHZ_512x480);      break;
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
