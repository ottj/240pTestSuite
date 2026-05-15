/*
 * 240p Test Suite - FM Towns port
 * Copyright (C) 2026 Artemio Urbina
 *
 * GPLv2 or later -- see LICENSE in repo root.
 */

#include "patterns.h"
#include "video.h"
#include "input.h"
#include <string.h>

/* ---- shared 16-colour palette ------------------------------------------ *
 *
 * Both the 8-bpp and 16-bpp paths talk about colours by index into this
 * table. In 8-bpp modes the indices are also the palette indices we
 * upload to the hardware DAC; in 16-bpp modes we pack the (r,g,b) into
 * a 15-bit RGB-555 value at draw time (Table I-4-3: bit 15 = transparent).
 *
 * The first 8 entries are the standard SMPTE color-bar order.
 */
typedef struct { u8 r, g, b; } RGB;

enum {
    C_WHITE = 0,
    C_YELLOW,
    C_CYAN,
    C_GREEN,
    C_MAGENTA,
    C_RED,
    C_BLUE,
    C_BLACK,
    C_GRAY,
    C_FG     /* "neutral foreground" -- light gray */
};

static const RGB pal[] = {
    { 255, 255, 255 },  /* 0 white   */
    { 255, 255,   0 },  /* 1 yellow  */
    {   0, 255, 255 },  /* 2 cyan    */
    {   0, 255,   0 },  /* 3 green   */
    { 255,   0, 255 },  /* 4 magenta */
    { 255,   0,   0 },  /* 5 red     */
    {   0,   0, 255 },  /* 6 blue    */
    {   0,   0,   0 },  /* 7 black   */
    { 128, 128, 128 },  /* 8 gray    */
    { 192, 192, 192 }   /* 9 fg      */
};
#define PAL_N ((int)(sizeof(pal) / sizeof(pal[0])))

/* Upload `pal` into the hardware 256-colour DAC for 8-bpp modes.
 * Indices 0..PAL_N-1 mirror this table; the rest stays whatever
 * video_set_mode() left in place.
 */
static void prime_palette(void)
{
    int i;
    for (i = 0; i < PAL_N; ++i)
        video_set_palette((u8)i, pal[i].r, pal[i].g, pal[i].b);
}

/* FM Towns 32768-colour pixel format. The hardware uses G-R-B ordering,
 * NOT the more common R-G-B (verified against the fmtowns_playground
 * `rgb15()` macro and the "paints the screen red" example writing
 * 0xAAAA -- which decodes to G=10, R=21, B=10 = reddish):
 *
 *   bit 15    : 0 (transparent flag, only used in superimpose composing)
 *   bits 14-10: G (5 bits)
 *   bits 9-5  : R (5 bits)
 *   bits 4-0  : B (5 bits)
 */
static u16 pack555(u8 r, u8 g, u8 b)
{
    return (u16)( ((u16)(g >> 3) << 10) | ((u16)(r >> 3) << 5) | (b >> 3) );
}

/* ---- bpp-aware primitives ---------------------------------------------- */

static void px(VideoSurface *s, int x, int y, u8 c)
{
    if ((unsigned)x >= s->width || (unsigned)y >= s->height) return;
    if (s->bpp == 8) {
        s->pixels[(u32)y * s->pitch + x] = c;
    } else {
        u16 v = pack555(pal[c].r, pal[c].g, pal[c].b);
        u8 *p = s->pixels + (u32)y * s->pitch + (u32)x * 2;
        p[0] = (u8)(v & 0xFF);
        p[1] = (u8)(v >> 8);
    }
}

static void hline(VideoSurface *s, int x0, int x1, int y, u8 c)
{
    int x;
    if ((unsigned)y >= s->height) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (x0 < 0) x0 = 0;
    if (x1 >= s->width) x1 = s->width - 1;

    if (s->bpp == 8) {
        u8 *row = s->pixels + (u32)y * s->pitch;
        for (x = x0; x <= x1; ++x) row[x] = c;
    } else {
        u16  v  = pack555(pal[c].r, pal[c].g, pal[c].b);
        u16 *row = (u16 *)(s->pixels + (u32)y * s->pitch);
        for (x = x0; x <= x1; ++x) row[x] = v;
    }
}

static void vline(VideoSurface *s, int x, int y0, int y1, u8 c)
{
    int y;
    if ((unsigned)x >= s->width) return;
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if (y0 < 0) y0 = 0;
    if (y1 >= s->height) y1 = s->height - 1;

    if (s->bpp == 8) {
        /* Step by pitch each row instead of recomputing y*pitch+x */
        u8 *p = s->pixels + (u32)y0 * s->pitch + x;
        for (y = y0; y <= y1; ++y) {
            *p = c;
            p += s->pitch;
        }
    } else {
        u16  v = pack555(pal[c].r, pal[c].g, pal[c].b);
        u8  *p = s->pixels + (u32)y0 * s->pitch + (u32)x * 2;
        for (y = y0; y <= y1; ++y) {
            *(u16 *)p = v;
            p += s->pitch;
        }
    }
}

static void fill(VideoSurface *s, u8 c)
{
    int y;
    for (y = 0; y < s->height; ++y) hline(s, 0, s->width - 1, y, c);
}

/* Visible / cycleable modes. The HFreq enum in video.h still defines
 * HFREQ_15KHZ_240P (book M11) and HFREQ_31KHZ_320x240 (book M10) for
 * reference, but those 2-screen 32768c configs don't render correctly
 * in our code (we only program one of the two layers), so they were
 * dropped from the menu and are skipped here too. Order matches the
 * menu's HC/256C grouping and resolution sort so the LEFT/RIGHT cycle
 * inside a pattern walks the same list a user sees in the menu.
 */
static const HFreq cycle_modes[] = {
    HFREQ_15KHZ_240P_256C,        /* CST 15K 240P 256C */
    HFREQ_15KHZ_480I,             /* M14 15K 480I 256C */
    HFREQ_24KHZ,                  /* M13 24K 640x400   */
    HFREQ_31KHZ,                  /* M12 31K 640x480   */
    HFREQ_15KHZ_240P_HC_1S,       /* CST 15K 240P HC   */
    HFREQ_31KHZ_320x240_HC_1S,    /* CST 31K 320x240HC */
    HFREQ_15KHZ_320x480,          /* M16 15K 320x480HC */
    HFREQ_31KHZ_320x480,          /* M15 31K 320x480HC */
    HFREQ_31KHZ_512x480,          /* M17 31K 512x480HC */
};
#define CYCLE_N ((int)(sizeof(cycle_modes) / sizeof(cycle_modes[0])))

/* Cycle to the prev/next mode in cycle_modes[]. After the mode change,
 * the patterns module gets a fresh blanked surface, so the caller must
 * re-render whatever it was showing.
 */
static void cycle_mode(int dir)
{
    HFreq cur = video_current_mode();
    int   idx = 0;
    int   i;
    for (i = 0; i < CYCLE_N; ++i) {
        if (cycle_modes[i] == cur) { idx = i; break; }
    }
    idx += dir;
    if (idx < 0)        idx = CYCLE_N - 1;
    if (idx >= CYCLE_N) idx = 0;
    video_set_mode(cycle_modes[idx]);
}

/* Wait one frame and poll input. Returns the bitmask of newly pressed
 * buttons so callers can react cleanly.
 *
 * Also handles the universal LEFT/RIGHT mode-cycle and CANCEL exit:
 * sets *out_redraw to 1 if the caller must re-render (mode changed),
 * sets *out_exit   to 1 if the caller should return.
 */
static u16 tick(int *out_redraw, int *out_exit)
{
    u16 pressed;
    video_wait_vblank();
    input_poll();
    pressed = input_pressed();

    *out_redraw = 0;
    *out_exit   = 0;

    if (pressed & PAD_LEFT)   { cycle_mode(-1); *out_redraw = 1; }
    if (pressed & PAD_RIGHT)  { cycle_mode(+1); *out_redraw = 1; }
    if (pressed & (PAD_CANCEL | PAD_QUIT)) *out_exit = 1;

    return pressed;
}

/* ---- color bars --------------------------------------------------------- */
/*
 * Eight vertical bars: white, yellow, cyan, green, magenta, red, blue, black
 * (SMPTE order). Colour indices match the first 8 entries of `pal`.
 */
static void render_color_bars(void)
{
    VideoSurface *s = video_get_surface();
    int y;
    int bar_w = s->width / 8;

    prime_palette();

    /* Draw row by row instead of column by column -- avoids one
     * multiply per pixel and lets the inner hline loop use a tight
     * memset/word-fill. Total work: height * 8 hline calls (= up to
     * a few thousand), vs the original width * height pixel writes.
     */
    for (y = 0; y < s->height; ++y) {
        int i;
        for (i = 0; i < 8; ++i) {
            int x0 = i * bar_w;
            int x1 = (i == 7) ? (s->width - 1) : (x0 + bar_w - 1);
            hline(s, x0, x1, y, (u8)i);
        }
    }
}

void pattern_color_bars(void)
{
    int redraw = 1, exit_now = 0;
    while (!exit_now) {
        if (redraw) render_color_bars();
        tick(&redraw, &exit_now);
    }
}

/* ---- grid --------------------------------------------------------------- */
/*
 * Pixel grid for geometry / convergence checks. Uses palette 70 (white)
 * on black. Spacing adapts to the active resolution so the grid always
 * looks square-ish: ~16-pixel cells on every mode.
 */
static void render_grid(void)
{
    VideoSurface *s = video_get_surface();
    int step = 16;
    int x, y;

    prime_palette();
    fill(s, C_BLACK);

    /* outer border */
    hline(s, 0, s->width  - 1, 0,             C_WHITE);
    hline(s, 0, s->width  - 1, s->height - 1, C_WHITE);
    vline(s, 0,             0, s->height - 1, C_WHITE);
    vline(s, s->width - 1,  0, s->height - 1, C_WHITE);

    /* grid lines */
    for (y = step; y < s->height - 1; y += step) hline(s, 0, s->width  - 1, y, C_WHITE);
    for (x = step; x < s->width  - 1; x += step) vline(s, x, 0, s->height - 1, C_WHITE);
}

void pattern_grid(void)
{
    int redraw = 1, exit_now = 0;
    while (!exit_now) {
        if (redraw) render_grid();
        tick(&redraw, &exit_now);
    }
}

/* ---- monoscope ---------------------------------------------------------- */
/*
 * Simple monoscope: centered crosshair, outer border, diagonal lines,
 * concentric rectangles. Good enough to check centering on each HFreq.
 */
static void rect(VideoSurface *s, int x0, int y0, int x1, int y1, u8 c)
{
    hline(s, x0, x1, y0, c);
    hline(s, x0, x1, y1, c);
    vline(s, x0, y0, y1, c);
    vline(s, x1, y0, y1, c);
}

static void line(VideoSurface *s, int x0, int y0, int x1, int y1, u8 c)
{
    int dx =  (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = -((y1 > y0) ? (y1 - y0) : (y0 - y1));
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        px(s, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        {
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
}

static void render_monoscope(void)
{
    VideoSurface *s = video_get_surface();
    int cx = s->width  / 2;
    int cy = s->height / 2;
    int i;

    prime_palette();
    fill(s, C_BLACK);

    /* concentric rectangles, 10% steps */
    for (i = 1; i <= 5; ++i) {
        int w = (s->width  * i) / 12;
        int h = (s->height * i) / 12;
        rect(s, cx - w, cy - h, cx + w, cy + h, C_WHITE);
    }

    /* diagonals corner-to-corner */
    line(s, 0,            0,             s->width - 1, s->height - 1, C_WHITE);
    line(s, s->width - 1, 0,             0,            s->height - 1, C_WHITE);

    /* center crosshair */
    hline(s, cx - 8, cx + 8, cy, C_WHITE);
    vline(s, cx, cy - 8, cy + 8, C_WHITE);
}

void pattern_monoscope(void)
{
    int redraw = 1, exit_now = 0;
    while (!exit_now) {
        if (redraw) render_monoscope();
        tick(&redraw, &exit_now);
    }
}

/* ---- solid colors ------------------------------------------------------- */
/*
 * Cycle full-screen primaries: white, red, green, blue, black.
 * UP/DOWN (or A) cycles within the pattern; LEFT/RIGHT swaps mode as usual.
 */
static const u8 solid_seq[] = {
    C_WHITE, C_RED, C_GREEN, C_BLUE, C_BLACK
};
#define SOLID_COUNT ((int)(sizeof(solid_seq) / sizeof(solid_seq[0])))

static void render_solid(int i)
{
    VideoSurface *s = video_get_surface();
    prime_palette();
    fill(s, solid_seq[i]);
}

void pattern_solid_colors(void)
{
    int idx = 0, redraw = 1, exit_now = 0;
    while (!exit_now) {
        u16 p;
        int dummy;
        if (redraw) render_solid(idx);
        p = tick(&redraw, &exit_now);
        if (p & (PAD_DOWN | PAD_A | PAD_RUN)) {
            idx = (idx + 1) % SOLID_COUNT;
            redraw = 1;
        } else if (p & PAD_UP) {
            idx = (idx - 1 + SOLID_COUNT) % SOLID_COUNT;
            redraw = 1;
        }
        (void)dummy;
    }
}

/* ---- rainbow ------------------------------------------------------------ *
 *
 * Linear colour sweep from the top-left corner (colour 0) to the
 * bottom-right corner (colour N-1). Adapted at runtime to the active
 * mode's resolution and colour depth:
 *
 *   - 8-bpp 256-colour modes load an RGB332 palette (3 bits R, 3 G,
 *     2 B, packed directly into the index) so the palette index
 *     itself encodes the RGB output of the DAC. The pixel value
 *     swept across the screen is the index.
 *   - 16-bpp 32768-colour modes write G-R-B-555 directly (Table I-4-3),
 *     bit 15 cleared. The pixel value swept across the screen is the
 *     packed RGB-15 word.
 *
 * Designed for digital-RGBHV signal probing on the Marty mainboard:
 * the pixel position deterministically encodes the value emitted by
 * the video controller, so a logic analyzer trace at any pixel can
 * be matched against the formula
 *
 *     colour(x,y) = ( (y * w + x) * N ) / (w * h)
 *
 * where N = 256 in 8-bpp modes and 32768 in 16-bpp modes.
 *
 * We avoid a 64-bit multiply (no compiler-rt linked) by using a
 * 16.16 fixed-point accumulator: step_fp = (N << 16) / total, then
 * colour = accum >> 16. The truncation costs at most ~1% of the
 * colour range at the bottom-right corner (e.g. 253 instead of 255
 * in 8-bpp 640x480) -- acceptable for our purposes since the formula
 * above is the authoritative mapping, not the screen contents.
 */

static void prime_rgb332_palette(void)
{
    int i;
    for (i = 0; i <= 255; ++i) {
        u8 v  = (u8)i;
        u8 r3 = (u8)((v >> 5) & 0x07);   /* bits 7..5 -> R */
        u8 g3 = (u8)((v >> 2) & 0x07);   /* bits 4..2 -> G */
        u8 b2 = (u8)( v       & 0x03);   /* bits 1..0 -> B */
        /* Replicate the bit-field across all 8 DAC bits so the
         * palette is monotonic and covers near-full intensity at
         * index 0xFF. */
        u8 r  = (u8)((r3 << 5) | (r3 << 2) | (r3 >> 1));
        u8 g  = (u8)((g3 << 5) | (g3 << 2) | (g3 >> 1));
        u8 b  = (u8)((b2 << 6) | (b2 << 4) | (b2 << 2) | b2);
        video_set_palette(v, r, g, b);
    }
}

static void render_rainbow(void)
{
    VideoSurface *s = video_get_surface();
    u32 total = (u32)s->width * (u32)s->height;
    int w = s->width;
    int x, y;

    if (s->bpp == 8) {
        u32 step_fp = (256U << 16) / total;
        u32 accum   = 0;
        prime_rgb332_palette();
        for (y = 0; y < s->height; ++y) {
            u8 *row = s->pixels + (u32)y * s->pitch;
            for (x = 0; x < w; ++x) {
                row[x] = (u8)(accum >> 16);
                accum += step_fp;
            }
        }
    } else {
        /* (32768 << 16) is exactly 2^31, fits unsigned. */
        u32 step_fp = (32768U << 16) / total;
        u32 accum   = 0;
        for (y = 0; y < s->height; ++y) {
            u16 *row = (u16 *)(s->pixels + (u32)y * s->pitch);
            for (x = 0; x < w; ++x) {
                /* Mask bit 15 defensively (superimpose transparency
                 * flag): truncation keeps accum <= 2^31 so the shift
                 * cannot set it, but a future change to ceiling
                 * rounding might. */
                row[x] = (u16)((accum >> 16) & 0x7FFFu);
                accum += step_fp;
            }
        }
    }
}

void pattern_rainbow(void)
{
    int redraw = 1, exit_now = 0;
    while (!exit_now) {
        if (redraw) render_rainbow();
        tick(&redraw, &exit_now);
    }
}
