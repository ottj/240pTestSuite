/*
 * 240p Test Suite - FM Towns port
 * Copyright (C) 2026 Artemio Urbina
 *
 * GPLv2 or later -- see LICENSE in repo root.
 *
 * --------------------------------------------------------------------------
 * FM Towns video driver
 * --------------------------------------------------------------------------
 *
 * The FM Towns video subsystem is built around a CRTC plus two display
 * layers (Sprite/Layer 0 and Layer 1) that can be composited or used
 * independently. Each layer reads from VRAM at a programmable address and
 * bit depth, and the CRTC drives the analog video output at one of three
 * horizontal frequencies: 15.7 kHz, 24.8 kHz, or 31.5 kHz.
 *
 * The CRTC is accessed through an indexed register pair:
 *
 *   I/O 0x440 (W) : index   (which CRTC register to access)
 *   I/O 0x442 (W) : data    (16-bit value to write to / read from)
 *
 * The video output controller ("Sprite/Layer mux") is accessed through:
 *
 *   I/O 0x448 (W) : index
 *   I/O 0x44A (W) : data
 *
 * The palette DAC for the 256-colour mode is accessed through:
 *
 *   I/O 0xFD90 (W) : palette index (0..255)
 *   I/O 0xFD92 (W) : blue  component (8-bit)
 *   I/O 0xFD94 (W) : red   component (8-bit)
 *   I/O 0xFD96 (W) : green component (8-bit)
 *
 * VRAM lives in the linear physical region starting at 0x80000000 (32 MiB
 * physical) -- in real mode we use the FMR/Towns BIOS to obtain a near-flat
 * mapping, or we run in protected mode with a flat data segment. The
 * Townsdev runtime sets up the latter and exposes VRAM as a normal pointer.
 *
 * The CRTC timing tables below were derived from the FM Towns Technical
 * Reference Manual and verified-by-eye against Tsugaru emulator. They drive
 * Layer 1 in 256-colour packed mode at the three canonical resolutions.
 *
 * Anything that has not yet been verified on real hardware is marked
 * with a "TODO(hw-verify)" comment so future passes can find it quickly.
 */

#include "video.h"
#include <string.h>

#if defined(__FMTOWNS__)
/* Bare-metal build (clang -target i386-elf) -- real port I/O. */
#include "boot/io.h"
#else
/* Host build (macOS clang) -- I/O is stubbed so the C still compiles
 * for unit testing the logic. Real test-target builds pass
 * -D__FMTOWNS__ and pull in the inline-asm helpers above.
 */
static inline void  outpw(u16 port, u16 v) { (void)port; (void)v; }
static inline void  outpb(u16 port, u8 v)  { (void)port; (void)v; }
__attribute__((unused)) static inline u16 inpw(u16 port) { (void)port; return 0; }
static inline u8    inpb (u16 port)        { (void)port; return 0; }
#endif

/* ---- I/O ports ---------------------------------------------------------- */

#define CRTC_INDEX   0x0440
#define CRTC_DATA    0x0442

#define VIDOUT_INDEX 0x0448
#define VIDOUT_DATA  0x044A

#define PAL_INDEX    0xFD90
#define PAL_BLUE     0xFD92
#define PAL_RED      0xFD94
#define PAL_GREEN    0xFD96

/* CRTC register file -- 32 16-bit registers at index 0x00..0x1F.
 * Programmed via index port 0x440 + data port 0x442.
 *
 * Values are copied verbatim from "FM TOWNS Technical Data Book"
 * (Fujitsu, ISBN 4-89052-393-2), Table I-4-23 "Register Setting Values".
 *
 *   Set 1  (Table I-4-21 mode 12)  640x480  256c    31.47 kHz
 *   Set 2  (Table I-4-21 mode 13)  640x400  256c    24.37 kHz
 *   Set 3  (Table I-4-21 mode 14)  720x480  256c    15.73 kHz interlace
 *   Set 14 (Table I-4-22 L0=11/L1=11) mode 11 240p  15.73 kHz NON-interlace
 *           Both layers 32768-colour, 4x H zoom, 128 logical pixels per
 *           line stretched to ~320 display pixels.
 *
 * SIFTER bytes (video-out mux regs 0..3) are taken directly from
 * Table I-4-23 footer (user-verified) and cross-checked against the live
 * code at github.com/gameblabla/fmtowns_playground (branch simp):
 *
 *   reg 0 (Control):
 *     256c   1-screen  : 0x0A  (Table I-4-23 sets 1, 2, 3 footer)
 *     32768c 1-screen  : 0x0F  (playground VIDEO_SET_31)
 *     16c    2-screen  : 0x15  (playground VIDEO_SET_9)
 *     32768c 2-screen  : 0x1F  (Table I-4-23 set 14 footer)
 *   reg 1 (Priority):
 *     256c   1-screen  : 0x18  (sets 1, 2, 3)
 *     32768c 2-screen  : 0x08  (set 14)
 *
 * The bit layout in Table I-4-35 (PMODE/CL11/CL10/CL01/CL00) does not
 * cleanly match these observed values via the obvious bit decoding;
 * rather than guess the field positions we use the verified values
 * verbatim.
 *
 * VRAM base address per Figs I-4-11 .. I-4-16:
 *   16c  640x400        : 0x80000000
 *   256c 1024x512 (1-scr): 0x80100000
 *   32768c 512x512 (1-scr): 0x80100000
 *   32768c 512x256 / 256x512 (2-scr): 0x80000000
 *
 * CR0 has bit 15 (START) OR'd in to enable display output.
 */

/* Which physical VRAM aperture a mode reads/writes through.
 *
 *   LAYER0 = "Layer 0 base" = Tsugaru TOWNSADDR_VRAM0_BASE.
 *            486+ : 0x80000000   386SX/Marty : 0x00A00000
 *   LAYER1 = "Layer 1 base" = Tsugaru TOWNSADDR_VRAM1_BASE.
 *            486+ : 0x80100000   386SX/Marty : 0x00B00000
 *
 * The 240p mode (mode 11, 2-screen 32768c) uses Layer 0; the three
 * 1-screen 256c modes (12/13/14) use Layer 1.
 */
typedef enum {
    VRAM_LAYER0 = 0,
    VRAM_LAYER1 = 1
} VRAMLayer;

typedef struct {
    u16        reg[0x20];   /* CRTC registers 0x00 .. 0x1F */
    u8         sifter[4];   /* video-out mux registers 0..3 */
    VRAMLayer  layer;       /* which VRAM aperture this mode draws to */
    u16        width;       /* logical pixels per VRAM line */
    u16        height;      /* lines per page */
    u16        pitch;       /* bytes per VRAM line */
    u8         bpp;         /* 8 or 16 */
} CRTCMode;

/* ---- mode 11 (240p): set 14 -- both layers = mode 11. ----------------- */
static const CRTCMode crtc_15khz_240p = {
    {
        0x0074, 0x0610, 0x0000, 0x0000,    /* 00 HSW1, 01 HSW2 (user-verified)  */
        0x0617, 0x0006, 0x000C, 0x0012,    /* 04 HST=0617, 05 VST1, 06 VST2, 07 EET */
        0x020B, 0x00E7, 0x05E7, 0x00E7,    /* 08 VST=020B (60Hz non-interlace),
                                              09 HDS0, 0A HDE0, 0B HDS1         */
        0x05E7, 0x002A, 0x020A, 0x002A,    /* 0C HDE1, 0D VDS0, 0E VDE0, 0F VDS1 */
        0x020A, 0x0000, 0x00E7, 0x0080,    /* 10 VDE1, 11 FA0,  12 HAJ0, 13 FO0  */
        0x0100, 0x0000, 0x00E7, 0x0080,    /* 14 LO0,  15 FA1,  16 HAJ1, 17 FO1  */
        0x0100, 0x0056, 0x0007, 0x0303,    /* 18 LO1,  19 EHAJ, 1A EVAJ,
                                              1B ZOOM = 4x H both layers         */
        0x8001, 0x0001, 0x0002, 0x0188     /* 1C CR0 (START|CL0=01: L0=32768c),
                                              1D CR1 (CLKSEL=01 -> 24.5454 MHz),
                                              1E FR, 1F CR2=0188                 */
    },
    { 0x1F, 0x08, 0x00, 0x00 },
    VRAM_LAYER0,                           /* mode 11 draws Layer 0 (32768c)    */
    /* width  = 320 logical 16-bpp pixels per row (= the virtual screen
     *          size per FM Towns Tech Data Book Table I-4-1 mode 11).
     *          The 4x H-zoom turns each row of 320 pixels into the
     *          HDE-HDS = 1280-dot active area (320*4 = 1280).
     * height = 240 rows.
     * pitch  = LO1 * 8 = 0x100 * 8 = 2048 bytes per VRAM line.
     */
    320,
    240,
    2048,
    16
};

/* ---- set 3: 15 kHz 480i 256-colour single-layer ------------------------ */
static const CRTCMode crtc_15khz_480i = {
    {
        0x0086, 0x0610, 0x0000, 0x0000,
        0x071B, 0x0006, 0x000C, 0x0012,
        0x020C, 0x0129, 0x06C9, 0x0129,
        0x06C9, 0x002A, 0x020A, 0x002A,
        0x020A, 0x0000, 0x0129, 0x0080,
        0x0100, 0x0000, 0x0129, 0x0080,
        0x0100, 0x0064, 0x0007, 0x0101,
        0x800F, 0x000C, 0x0003, 0x01CA
    },
    { 0x0A, 0x18, 0x00, 0x00 },
    VRAM_LAYER1,                          /* 256c 1-screen aperture            */
    /* Mode 14 is 480i per book Table I-4-1, but in Tsugaru's emulation
     * the two interlace fields appear to read the same VRAM region
     * (FO doesn't seem to take effect, or both fields collapse to one),
     * so the screen shows our content twice vertically -- once per
     * field. Real Marty hardware may render this correctly. For now
     * we present a 240-row surface with stride = LO1 * 8 = 2048 (the
     * book's per-field-line size), accepting the Tsugaru duplication
     * as a known emulator quirk. See boot/README.md for details.
     */
    720,                                  /* width  */
    240,                                  /* height (one field; CRTC doubles) */
    2048,                                 /* pitch (= LO1 * 8) */
    8                                     /* bpp    */
};

/* ---- set 2: 24 kHz 640x400 256c --------------------------------------- */
static const CRTCMode crtc_24khz_640x400 = {
    {
        0x0040, 0x0320, 0x0000, 0x0000,
        0x035F, 0x0000, 0x0010, 0x0000,
        0x036F, 0x009C, 0x031C, 0x009C,
        0x031C, 0x0040, 0x0360, 0x0040,
        0x0360, 0x0000, 0x009C, 0x0000,
        0x0080, 0x0000, 0x009C, 0x0000,
        0x0080, 0x004A, 0x0001, 0x0000,
        0x800F, 0x0003, 0x0000, 0x0150
    },
    { 0x0A, 0x18, 0x00, 0x00 },
    VRAM_LAYER1,
    /* mode 13: LO1 = 0x80 -> pitch = 0x80 * 8 = 1024 bytes per VRAM line */
    640,
    400,
    1024,
    8
};

/* ---- set 1: 31 kHz 640x480 256c --------------------------------------- */
static const CRTCMode crtc_31khz_640x480 = {
    {
        0x0060, 0x02C0, 0x0000, 0x0000,
        0x031F, 0x0000, 0x0004, 0x0000,
        0x0419, 0x008A, 0x030A, 0x008A,
        0x030A, 0x0046, 0x0406, 0x0046,
        0x0406, 0x0000, 0x008A, 0x0000,
        0x0080, 0x0000, 0x008A, 0x0000,
        0x0080, 0x0058, 0x0001, 0x0000,
        0x800F, 0x0002, 0x0000, 0x0192
    },
    { 0x0A, 0x18, 0x00, 0x00 },
    VRAM_LAYER1,
    /* mode 12: LO1 = 0x80 -> pitch = 0x80 * 8 = 1024 bytes per VRAM line.
     * Virtual screen is 1024x512 per FM Towns Tech Data Book; we use
     * only the 640x480 visible region.
     */
    640,
    480,
    1024,
    8
};

/* ===== 32768-colour ("high colour") modes ============================ */

/* Mode 10: 31 kHz, 320x240 32768-colour (2-screen layout, Layer 0 active).
 * Values from gameblabla/fmtowns_playground CRTC_SET_28.
 * Pitch: LO1 * 8 = 0x100 * 8 = 2048 bytes per VRAM line.
 */
static const CRTCMode crtc_31khz_320x240_hc = {
    {
        0x0060, 0x02C0, 0x0000, 0x0000,
        0x031F, 0x0000, 0x0004, 0x0000,
        0x0419, 0x008A, 0x01CA, 0x008A,
        0x01CA, 0x0046, 0x0226, 0x0046,
        0x0226, 0x0000, 0x008A, 0x0000,
        0x0100, 0x0000, 0x008A, 0x0000,
        0x0100, 0x0058, 0x0000, 0x0000,
        0x8005, 0x0002, 0x0000, 0x0192    /* CR0: 2-screen 32768c/32768c, START */
    },
    { 0x1F, 0x08, 0x00, 0x00 },           /* video-out: 2-screen 32768c+32768c */
    VRAM_LAYER0,
    320,
    240,
    2048,
    16
};

/* Mode 15: 31 kHz, 320x480 32768-colour 1-screen.
 * Book Table I-4-23 set 4 (Table I-4-21 mode 15 -> set 4).
 */
static const CRTCMode crtc_31khz_320x480_hc = {
    {
        0x0060, 0x02C0, 0x0000, 0x0000,
        0x031F, 0x0000, 0x0004, 0x0000,
        0x0419, 0x008A, 0x030A, 0x008A,
        0x030A, 0x0046, 0x0406, 0x0046,
        0x0406, 0x0000, 0x008A, 0x0000,
        0x0080, 0x0000, 0x008A, 0x0000,
        0x0080, 0x0058, 0x0001, 0x0101,   /* ZOOM: 2x H both layers */
        0x800A, 0x0002, 0x0000, 0x0192    /* CR0: 1-screen 32768c */
    },
    { 0x0A, 0x18, 0x00, 0x00 },           /* video-out: 1-screen 32768c */
    VRAM_LAYER1,
    320,
    480,
    1024,                                 /* LO1 * 8 */
    16
};

/* Mode 16: 15 kHz, 320x480 32768-colour, interlace.
 * Book Table I-4-23 set 5 (Table I-4-21 mode 16 -> set 5).
 */
static const CRTCMode crtc_15khz_320x480_hc = {
    {
        0x0074, 0x0530, 0x0000, 0x0000,
        0x0617, 0x0006, 0x000C, 0x0012,
        0x020C, 0x00E7, 0x05E7, 0x00E7,
        0x05E7, 0x002A, 0x020A, 0x002A,
        0x020A, 0x0000, 0x00E7, 0x0080,
        0x0100, 0x0000, 0x00E7, 0x0080,
        0x0100, 0x0056, 0x0007, 0x0303,   /* ZOOM: 4x H both layers */
        0x800A, 0x0001, 0x0000, 0x0188    /* CR0: 1-screen 32768c, CR1 CLKSEL=01 */
    },
    { 0x0A, 0x18, 0x00, 0x00 },
    VRAM_LAYER1,
    320,
    240,                                  /* See 480i comment: 240 unique rows */
    2048,                                 /* LO1 * 8 */
    16
};

/* Mode 17: 31 kHz, 512x480 32768-colour 1-screen.
 * Values from gameblabla/fmtowns_playground CRTC_SET_31 (the FM Towns
 * flagship high-colour mode -- used by most demo/showcase software).
 */
static const CRTCMode crtc_31khz_512x480_hc = {
    {
        0x0060, 0x02C0, 0x0000, 0x0000,
        0x031F, 0x0000, 0x0004, 0x0000,
        0x0419, 0x00CA, 0x02CA, 0x00CA,
        0x02CA, 0x0046, 0x0406, 0x0046,
        0x0406, 0x0000, 0x00CA, 0x0000,
        0x0080, 0x0000, 0x00CA, 0x0000,
        0x0080, 0x0058, 0x0001, 0x0000,
        0x800A, 0x0002, 0x0000, 0x0192    /* CR0: 1-screen 32768c, START */
    },
    { 0x0A, 0x18, 0x00, 0x00 },           /* video-out: 1-screen 32768c */
    VRAM_LAYER1,
    512,
    480,
    1024,                                 /* LO1 * 8 */
    16
};

/* ---- Custom: 15 kHz 240p 256c (NOT in the book) ------------------------ *
 *
 * The book defines 256-colour modes only at 24/31 kHz progressive or
 * at 15 kHz 480i (modes 12, 13, 14). To give Marty users a 256-colour
 * mode that drives the TV at proper 240p we derive a custom CRTC config
 * from mode 11 (which IS 240p but in 32768c 2-screen) and flip the
 * layer/colour-depth bits to "256c 1-screen":
 *
 *   - All timing registers (HSW, HST, VST, HDS/HDE, VDS/VDE, ...) come
 *     verbatim from mode 11, so the analog output is identical 15 kHz
 *     240p as far as the CRT is concerned.
 *   - CR0 -> 0x800F (matches modes 12/13/14 = START | 256c-1-screen).
 *   - SIFTER -> { 0x0A, 0x18, 0x00, 0x00 } (256c-1-screen).
 *   - VRAM aperture -> Layer 1.
 *   - bpp -> 8.
 *   - width = 320 logical bytes/line; the 4x H zoom from mode 11 expands
 *     those 320 bytes into the 1280-dot active area (HDE-HDS = 0x500).
 *
 * Untested. Worst case: the CRTC doesn't accept this mix of timing +
 * layer config and the display goes black or wraps weirdly. If it
 * works it gives Marty owners a proper 240p 256c mode for TV testing.
 */
static const CRTCMode crtc_15khz_240p_256c = {
    {
        0x0074, 0x0610, 0x0000, 0x0000,   /* HSW1, HSW2 (same as mode 11)  */
        0x0617, 0x0006, 0x000C, 0x0012,   /* HST, VST1, VST2, EET          */
        0x020B, 0x00E7, 0x05E7, 0x00E7,   /* VST=020B (60Hz non-interlace),
                                             HDS0, HDE0, HDS1              */
        0x05E7, 0x002A, 0x020A, 0x002A,   /* HDE1, VDS0, VDE0, VDS1        */
        0x020A, 0x0000, 0x00E7, 0x0080,   /* VDE1, FA0, HAJ0, FO0          */
        0x0100, 0x0000, 0x00E7, 0x0080,   /* LO0, FA1, HAJ1, FO1           */
        0x0100, 0x0056, 0x0007, 0x0303,   /* LO1, EHAJ, EVAJ, ZOOM=4x H    */
        0x800F, 0x0001, 0x0002, 0x0188    /* CR0=256c-1-screen|START,
                                             CR1=CLKSEL01, FR, CR2         */
    },
    { 0x0A, 0x18, 0x00, 0x00 },           /* 256c 1-screen video-out mux   */
    VRAM_LAYER1,
    320,                                  /* width: 320 logical 8-bpp     */
    240,                                  /* height: 240 unique rows      */
    2048,                                 /* pitch = LO1 * 8              */
    8
};

/* ---- Custom: 15 kHz 240p 32768c on a single layer ---------------------- *
 *
 * Mode 11 as defined by the book (set 14) is a 2-screen 32768c config
 * where both Layer 0 and Layer 1 contribute to each displayed pixel.
 * Our patterns code only programs Layer 0, leaving Layer 1 with
 * uninitialised VRAM, which is why the book-spec mode 11 renders as
 * "blown up and only partially visible" on both Tsugaru and real Marty.
 *
 * This custom config keeps mode 11's full 240p timing (HSW, HST, VST,
 * HDS/HDE, VDS/VDE, ZOOM=4x H, FR non-interlace) and just flips the
 * layer config to "1-screen 32768c on Layer 1":
 *
 *   - CR0    : 0x8001 -> 0x800A  (matches modes 15/16/17 1-screen HC)
 *   - SIFTER : 0x1F/0x08 -> 0x0A/0x18 (1-screen HC video-out mux)
 *   - aperture: Layer 0 -> Layer 1
 *
 * The analog signal at the connector is the same 15 kHz / 240p timing
 * that mode 11 produces; the only difference is that the picture data
 * comes entirely from one layer. Marty TV output should now show our
 * full content instead of being broken by the missing second layer.
 */
static const CRTCMode crtc_15khz_240p_hc_1s = {
    {
        0x0074, 0x0610, 0x0000, 0x0000,   /* same horizontal timing as mode 11 */
        0x0617, 0x0006, 0x000C, 0x0012,
        0x020B, 0x00E7, 0x05E7, 0x00E7,
        0x05E7, 0x002A, 0x020A, 0x002A,
        0x020A, 0x0000, 0x00E7, 0x0080,
        0x0100, 0x0000, 0x00E7, 0x0080,
        0x0100, 0x0056, 0x0007, 0x0303,   /* ZOOM = 4x H both layers */
        0x800A, 0x0001, 0x0002, 0x0188    /* CR0 = 1-screen 32768c, FR=240p */
    },
    { 0x0A, 0x18, 0x00, 0x00 },           /* 1-screen 32768c video-out mux */
    VRAM_LAYER1,
    320,                                  /* 320 logical 16-bpp pixels per row */
    240,
    2048,                                 /* pitch = LO1 * 8 */
    16
};

static const CRTCMode *const mode_table[HFREQ_COUNT] = {
    &crtc_15khz_240p,
    &crtc_15khz_480i,
    &crtc_24khz_640x400,
    &crtc_31khz_640x480,
    &crtc_31khz_320x240_hc,
    &crtc_31khz_320x480_hc,
    &crtc_15khz_320x480_hc,
    &crtc_31khz_512x480_hc,
    &crtc_15khz_240p_256c,
    &crtc_15khz_240p_hc_1s,
};

static const char *const mode_name[HFREQ_COUNT] = {
    "15 kHz / 240p / 32768c (mode 11)",
    "15 kHz / 480i / 256c   (mode 14)",
    "24 kHz / 640x400 256c  (mode 13)",
    "31 kHz / 640x480 256c  (mode 12)",
    "31 kHz / 320x240 32768c (mode 10)",
    "31 kHz / 320x480 32768c (mode 15)",
    "15 kHz / 320x480 32768c (mode 16)",
    "31 kHz / 512x480 32768c (mode 17)",
    "15 kHz / 240p / 256c   (custom)",
    "15 kHz / 240p / 32768c (custom 1-screen)"
};

/* ---- Linear framebuffer ------------------------------------------------- *
 *
 * VRAM bases are CPU-dependent.
 *
 *   486+ Towns       Layer 0: 0x80000000   Layer 1: 0x80100000
 *   386SX / Marty    Layer 0: 0x00A00000   Layer 1: 0x00B00000
 *
 * We pick the right pair at video_init() time by reading the machine-ID
 * I/O port at 0x30. The low 2 bits encode the CPU type (per FM Towns
 * Technical Data Book pp.775/781 and Tsugaru's MachineID() function):
 *   00 = 80286, 01 = 80386DX, 10 = 80486/Pentium, 11 = 80386SX.
 */
static u32 g_vram_base[2] = { 0x80000000UL, 0x80100000UL };  /* 486+ defaults */

#if defined(__FMTOWNS__)
static inline u8 *vram_ptr(VRAMLayer layer) {
    return (u8 *)(uintptr_t)g_vram_base[(int)layer];
}
#else
static u8 vram_stub[720 * 480 * 2];
static inline u8 *vram_ptr(VRAMLayer layer) { (void)layer; return vram_stub; }
#endif

static VideoSurface g_surf;
static HFreq        g_mode = HFREQ_15KHZ_240P;

/* ---- CRTC helpers ------------------------------------------------------- */
/*
 * Programming pattern mirrors captainys/FM IPL CRTC_SETREG:
 *   1. write CRTC regs 0x00, 0x01 (HSW1, HSW2) -- ports 0x440/0x442
 *   2. write CRTC regs 0x04..0x1F                -- same ports
 *   3. write 4-byte "sifter" via the video-out mux -- ports 0x448/0x44A
 *      (the video-out mux register indices 0x00, 0x01, 0x02, 0x03)
 *
 * Note: regs 0x02 and 0x03 are deliberately skipped -- the IPL never
 * writes them, and writing 0 there changes nothing on the chips we know.
 */
static void crtc_program(const CRTCMode *m)
{
    int i;

    /* Pass 1: registers 0x00..0x01 */
    for (i = 0x00; i <= 0x01; ++i) {
        outpw(CRTC_INDEX, (u16)i);
        outpw(CRTC_DATA,  m->reg[i]);
    }
    /* Pass 2: registers 0x04..0x1F */
    for (i = 0x04; i <= 0x1F; ++i) {
        outpw(CRTC_INDEX, (u16)i);
        outpw(CRTC_DATA,  m->reg[i]);
    }
    /* Sifter (video-out mux regs 0..3): index port = 0x448, data = 0x44A,
     * 8-bit accesses. */
    for (i = 0; i < 4; ++i) {
        outpb(VIDOUT_INDEX, (u8)i);
        outpb(VIDOUT_DATA,  m->sifter[i]);
    }
}

/* ---- Public API --------------------------------------------------------- */

void video_init(void)
{
    /* Identify the CPU class so we know which physical aperture VRAM
     * lives at. Reading I/O port 0x30 returns a byte whose low 2 bits
     * encode the CPU class (11 = 80386SX). The 386SX has only a 24-bit
     * physical bus, so VRAM is mapped under 16 MB.
     */
    u8 mid = inpb(0x0030);
    if ((mid & 0x03) == 0x03) {
        g_vram_base[VRAM_LAYER0] = 0x00A00000UL;
        g_vram_base[VRAM_LAYER1] = 0x00B00000UL;
    } else {
        g_vram_base[VRAM_LAYER0] = 0x80000000UL;
        g_vram_base[VRAM_LAYER1] = 0x80100000UL;
    }

    g_mode = HFREQ_24KHZ;
}

void video_shutdown(void)
{
    /* Leave the machine in 24 kHz mode so TownsOS / DOS shells render
     * normally on a Towns monitor. The Marty user will see the splash
     * screen come back on TV regardless.
     */
    video_set_mode(HFREQ_24KHZ);
}

int video_set_mode(HFreq f)
{
    const CRTCMode *m;
    if ((unsigned)f >= HFREQ_COUNT)
        return -1;

    m = mode_table[f];
    crtc_program(m);

    g_mode         = f;
    g_surf.pixels  = vram_ptr(m->layer);
    g_surf.width   = m->width;
    g_surf.height  = m->height;
    g_surf.pitch   = m->pitch;
    g_surf.bpp     = m->bpp;

    /* Palette is only meaningful for 8-bpp (256-colour) modes.
     * For 32768-colour modes the pixel itself encodes the colour and
     * we skip palette programming entirely.
     */
    if (m->bpp == 8) {
        int i;
        for (i = 0; i < 64; ++i) {
            u8 v = (u8)(i * 255 / 63);
            video_set_palette((u8)i, v, v, v);
        }
        video_set_palette(64, 255,   0,   0);
        video_set_palette(65,   0, 255,   0);
        video_set_palette(66,   0,   0, 255);
        video_set_palette(67, 255, 255,   0);
        video_set_palette(68,   0, 255, 255);
        video_set_palette(69, 255,   0, 255);
        video_set_palette(70, 255, 255, 255);
        video_set_palette(71,   0,   0,   0);
    }

    video_clear(0);
    return 0;
}

HFreq video_current_mode(void)        { return g_mode; }
const char *video_mode_name(HFreq f)  { return (unsigned)f < HFREQ_COUNT ? mode_name[f] : "?"; }
VideoSurface *video_get_surface(void) { return &g_surf; }

void video_set_palette(u8 index, u8 r, u8 g, u8 b)
{
    outpb(PAL_INDEX, index);
    outpb(PAL_BLUE,  b);
    outpb(PAL_RED,   r);
    outpb(PAL_GREEN, g);
}

void video_clear(u8 index)
{
    u32 n = (u32)g_surf.pitch * g_surf.height;
    memset(g_surf.pixels, index, n);
    /* For 16-bpp the result is a pixel where both bytes equal `index`.
     * Patterns clear via a higher-level helper that does the right thing;
     * this function exists mainly to wipe VRAM to "black" between modes.
     */
}

/* Vsync.
 * IODEF.ASM in captainys/FM IPL puts the VSYNC status at I/O 0xFDA0.
 * Bit 0 high = currently in vsync (active blanking). We wait for the
 * trailing edge then the leading edge so each call returns aligned to
 * the start of a frame.
 */
#define IO_VSYNC 0xFDA0

void video_wait_vblank(void)
{
    while ( (inpb(IO_VSYNC) & 0x01)) { }   /* leave vsync */
    while (!(inpb(IO_VSYNC) & 0x01)) { }   /* re-enter vsync = new frame */
}
