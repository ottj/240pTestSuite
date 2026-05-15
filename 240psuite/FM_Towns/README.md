# 240p Test Suite -- FM Towns / FM Towns Marty

A minimal first cut of the 240p Test Suite for the Fujitsu FM Towns family.

The goal of this version is narrow on purpose:

* boot on a stock FM Towns and on an FM Towns Marty,
* switch the CRTC between all three horizontal frequencies the system
  supports (15.7 kHz / 24.8 kHz / 31.5 kHz),
* let the user compare the same test pattern across those frequencies
  without leaving the pattern,
* be controllable purely through the joypad (so it works on a Marty,
  which has no keyboard).

It is **not** a port of the full Genesis/SNES test suite -- only the
patterns most useful for verifying that the resolution switch worked
correctly on each output: color bars, grid, monoscope, solid colors,
plus a deterministic full-colour-space rainbow that doubles as a
saturation / gradient sweep and a probe-friendly stimulus for hardware
analysis.

## Layout

| File         | Purpose |
|--------------|---------|
| `main.c`     | Entry point. Brings up video + input and runs the menu. |
| `video.[ch]` | CRTC programming, mode tables, palette, vsync. CPU-aware VRAM bases (`0x80000000`/`0x80100000` on 486+, `0xA00000`/`0xB00000` on 386SX/Marty). |
| `input.[ch]` | FM Towns / Marty pad reading + keyboard fallback. |
| `menu.[ch]`  | Top-level menu UI. |
| `patterns.[ch]` | Color bars / grid / monoscope / solid colors / rainbow. Bpp-aware: 8-bpp palette indices in 256c modes, packed 16-bpp G-R-B-555 words in HC modes, from the same drawing code. |
| `font.[ch]`  | Tiny built-in 8x8 ASCII font for the menu. |
| `Makefile`   | (Stub for the original Townsdev path; not used by the working bare-metal build.) |
| `boot/`      | **Working build path**: clang -target i386-elf + NASM IPL + LLD. Produces a bootable CD ISO. See `boot/README.md`. |

## Building (current working path)

```
cd boot
make cd
BOOT=cd ./run.sh /path/to/marty-rom-dir
```

This builds `boot/cdimage.iso` — a 1 MB CD image with our IPL+kernel
in the first 8 KB, ready for Tsugaru_CUI. Should also work on real
Marty hardware once burned to a CD.

See `boot/README.md` for the full boot pipeline, Marty-specific
findings (386SX VRAM apertures, BIOS quirks, etc.), and the build
toolchain (clang + LLD + NASM).

## Controls

| FM Towns pad | Action |
|--------------|--------|
| D-pad UP / DOWN  | move menu cursor |
| D-pad LEFT/RIGHT | (inside a pattern) cycle through all 4 video modes |
| A button     | confirm / activate menu item |
| B button     | leave the current pattern |
| RUN (= LEFT+RIGHT chord) | alt confirm |
| SELECT (= UP+DOWN chord) | alt cancel |

The FM Towns pad signals RUN as a simultaneous LEFT+RIGHT chord and
SELECT as UP+DOWN (Table I-7-9). The driver detects both.

If you have a keyboard plugged in (i.e. a real FM Towns rather than a
Marty), arrow keys + Space/Return/ESC mirror the pad.

## Video modes

The menu exposes eleven modes: four 256-colour and four 32768-colour
drawn from the FM Towns Technical Data Book's standard mode table,
plus three custom configs not in the book (a 256C 240p and 1-screen
variants of modes 11 and 10). Behaviour is annotated against both
Tsugaru-Marty emulation **and** real FM Towns Marty hardware via
composite / S-Video off the on-board downscaler ASIC -- the only
output paths Marty exposes natively.

Menu order is by colour depth (256C first, then HC), then by
resolution starting at 240p with TV frequencies before monitor.
Custom variants appear before their book-mode counterparts so
working modes are at the top of each block.

| Menu label             | Book mode | Reg set | Pixel layout                                                  | Tsugaru-Marty                  | Real Marty (composite/S-Video) |
|------------------------|-----------|---------|---------------------------------------------------------------|--------------------------------|--------------------------------|
| CST 15K 240P 256C      | --        | custom  | 320x240 x 8 bpp at 15 kHz non-interlace (mode 11 timing + 256c 1-screen on Layer 1) | -- | **works** -- proper 240p 256-colour TV mode, not in the book |
| M14 15K 480I 256C      | mode 14   | set 3   | 720x480 visible x 8 bpp (256c, interlaced)                    | doubles vertically (FO ignored)| **works**, flickers as expected for 480i |
| M13 24K 640X400 *      | mode 13   | set 2   | 640x400 x 8 bpp                                               | works                          | **works**, menu legible at 2x font scale |
| M12 31K 640X480 *      | mode 12   | set 1   | 640x480 x 8 bpp                                               | works (menu mode)              | **works**, menu legible at 2x font scale |
| CST 15K 240P  HC       | --        | custom  | 320x240 x 16 bpp at 15 kHz non-interlace (mode 11 timing + 32768c 1-screen on Layer 1) | vertically doubled (same Tsugaru 15 kHz quirk as M14) | **works** -- proper 240p 32768c TV mode; colours wrong via composite because of the Marty ASIC, not our code |
| M11 15K 240P  HC       | mode 11   | set 14  | book-spec 2-screen 32768c, 4x H-zoom -> ~320x240 effective TV | crops vertically               | **blown up, only partially visible** -- we only program Layer 0; the 2-screen layer split mangles the picture. Use `CST 15K 240P HC` for a working 240p 32768c instead. |
| CST 31K 320X240HC      | --        | custom  | 320x240 x 16 bpp at 31 kHz (mode 15 timing + 2x V zoom, 32768c 1-screen on Layer 1) | -- | **works** -- 320x240 32768c upscaled by the CRTC to fill the full 640x480 visible area on a 31 kHz monitor |
| M10 31K 320X240HC      | mode 10   | --      | book-spec 2-screen 32768c at 31 kHz, 320 dot x 240 line active area | 2-screen compositing artefacts | **shifted left, only top-left quadrant visible** -- two issues: (1) 2-screen layer config (only Layer 0 programmed), (2) the active area is only 320x240 dots on a 640x480 monitor, so the picture occupies one quadrant. Use `CST 31K 320X240HC` for a working 320x240 32768c. |
| M16 15K 320X480HC      | mode 16   | --      | 320x480 x 16 bpp (32768c, 15 kHz interlace)                   | vertically doubled             | **wrong colours** (ASIC truncation) |
| M15 31K 320X480HC      | mode 15   | --      | 320x480 x 16 bpp (32768c)                                     | vertically squished            | **wrong colours** (ASIC truncation) |
| M17 31K 512X480 *      | mode 17   | --      | 512x480 x 16 bpp (32768c, "flagship")                         | works                          | **wrong colours** (ASIC truncation) |

The pattern of the three `CST` modes is the same family of fix: take
a broken book mode that wanted 2-screen 32768c, swap the layer config
to "1-screen on Layer 1" so all of the picture comes from a single
VRAM aperture. For M11 the layer flip alone is enough -- mode 11's
timing already gives a TV-sized picture (1280 dot active area at
24.5 MHz / 4x H zoom = 320 logical source pixels filling the NTSC
visible area). For M10 the timing itself was also too small -- a
320 x 240 active area sat in one quadrant of a 31 kHz monitor -- so
we additionally borrow mode 15's 640 x 480 active area and add 2x V
zoom on top of mode 15's 2x H zoom, upscaling a 320 x 240 source to
fill the full visible monitor area. The corresponding book modes
(M11, M10) are kept in the menu as reference for anyone who wants
to revisit the 2-screen layer setup.

Note that the **HC-mode colour truncation on real Marty** is a Marty
hardware constraint: the downscaler ASIC mangles the 32768-colour
output before it reaches composite/S-Video. Owners of non-Marty FM
Towns models with proper RGB output should see HC modes correctly;
Marty owners can either accept the composite truncation or probe the
digital RGB lines upstream of the ASIC and feed an external display,
which is what the [RAINBOW pattern](#rainbow-deterministic-colour-space-sweep)
is convenient for.

The 240p mode (mode 11) is the canonical FM Towns Marty TV-out mode. It
uses both display layers configured in 32768-colour direct mode, with
Layer 0 active and 4x horizontal zoom on both layers; LO1=0x100=256
bytes/line gives 128 logical pixels per VRAM line stretched to ~320 dots.
The pattern getting cropped on **both** Tsugaru and real hardware
strongly suggests our CRTC parameters for mode 11 are off rather than
this being an emulator artefact.

The 32768-colour pixel format is **G-R-B-555** (bits 14-10 = green,
9-5 = red, 4-0 = blue, bit 15 reserved for the superimpose transparency
flag -- Table I-4-3, cross-checked against the `rgb15()` macro in
fmtowns_playground). `patterns.c` is bpp-aware: it writes 8-bit palette
indices in the 256-colour modes and packed 16-bit G-R-B-555 pixels in
the high-colour modes from the same drawing code.

## Patterns

`COLOR BARS`, `GRID`, `MONOSCOPE`, `SOLID COLORS` are the classic
geometry / convergence / colour-fidelity tests. Inside any pattern,
**LEFT / RIGHT cycles through the eight video modes** so the same
pattern can be compared across all three horizontal frequencies and
both colour depths without leaving the pattern. **B / SELECT / ESC**
returns to the menu.

### Rainbow (deterministic colour-space sweep)

The `RAINBOW` pattern is **not** a perceptual rainbow. It is a
linear walk through the colour space in **bit-significance order**,
so the pixel value at any known screen position is deterministic and
bit-reversible from its `(x, y)` coordinate. Two practical uses:

- As a **saturation / gradient sweep**: every available colour shows
  up at least once, useful for checking that the active mode actually
  reaches the colour depth it claims (256 vs 32768 distinct values).
- As a **probe-friendly stimulus** for hardware analysis: because
  `colour(x, y)` is given by a closed-form formula (below), a logic
  analyzer trace on the digital RGB lines at a known pixel position
  can be matched against the expected DAC output. On Marty this is
  the only way to confirm 15-bit colour, since the on-board ASIC
  truncates it before composite/S-Video.

The mapping is:

```
colour(x, y) = ( (y * w + x) * N ) / (w * h)
```

with `N = 256` in 256-colour modes, `N = 32768` in HC modes, and
`w`, `h` the active mode's width and height. The implementation uses
a 16.16 fixed-point accumulator (no compiler-rt is linked, so 64-bit
multiplies are out); truncation costs at most one or two colour
indices at the bottom-right corner pixel, e.g. 253 instead of 255 in
8-bpp 640x480. The formula above is the authoritative mapping --
match probe traces against it, not against the literal end colour.

#### 256-colour modes: RGB332 palette

Before drawing, the pattern uploads an **RGB332 palette** so that
the palette index itself encodes the bits the DAC will emit:

```
index bits  7 6 5  4 3 2  1 0
field       R R R  G G G  B B
```

Each field is replicated across the 8 DAC output bits so the palette
is monotonic and reaches near-full intensity at index `0xFF`. If you
see palette index `0xE0` on the bus, the DAC should be driving
(R = 0xFF, G = 0, B = 0).

#### High-colour modes: G-R-B-555 direct

The pattern writes the packed 16-bit word straight into VRAM, with
bit 15 cleared (the superimpose transparency flag, Table I-4-3):

```
word bits  15  14 13 12 11 10  9 8 7 6 5  4 3 2 1 0
field      -    G  G  G  G  G  R R R R R  B B B B B
```

Pixel word `0x7C00` decodes to (G = 31, R = 0, B = 0) = pure green.

#### Why it looks like stripes

Because the counter increments by 1 per pixel, the **lowest bits cycle
fastest** and the **highest bits cycle slowest**. That maps directly
to the visible geometry:

In **HC modes** (16 bpp G-R-B-555), G is the most significant field, so
the screen breaks into **32 large horizontal stripes** corresponding to
G = 0..31. Within each stripe, R increments top-to-bottom (a few rows
per R value), and B sweeps 0..31 several times across each row -- that
B sweep is what shows up as the diagonal staircase, because 32 B-steps
generally don't align with the screen width:

| Field | Bits | Cycles every | Visual effect |
|-------|------|--------------|---------------|
| B     | 0..4   | 32 colour steps  | fast horizontal sweep within each row (diagonal-looking due to row-misalignment) |
| R     | 5..9   | 1024 colour steps | a few horizontal R-bands inside each G-stripe |
| G     | 10..14 | 32768 colour steps | 32 large horizontal stripes, top→bottom |

So in the **first stripe** (G = 0): top rows have low R (dark / blue
shades), middle rows have mid R (dim red), bottom rows have R = 31
(pure red on the left of each B-sweep, magenta on the right where B
peaks). In the **last stripe** (G = 31): the left of each B-sweep is
pure green, the middle is yellow (G + R), the right is white-ish
(G + R + B).

In **256C modes** (8 bpp RGB332) the same logic applies with fewer
bits, so you see **8 large horizontal stripes** corresponding to
R = 0..7, each with 8 internal G sub-bands of 4 B values. Same purpose,
fewer macro-stripes.

#### How to use it for probing

1. Run the test suite, navigate to `RAINBOW`, optionally cycle to the
   mode you want to probe (LEFT/RIGHT).
2. Pick a pixel position `(x, y)`.
3. Compute `idx = y * w + x`, then `colour = idx * N / (w * h)`.
4. Decode `colour` against the bit layout for that mode (RGB332 or
   G-R-B-555).
5. The decoded `(R, G, B)` triple is what the video controller is
   driving on the digital RGB output for that pixel. Compare with the
   logic analyzer trace.

## Hardware notes & caveats

`video.c` and `input.c` are now sourced from the **FM TOWNS Technical
Data Book** (Fujitsu, ISBN 4-89052-393-2). Every magic number traces
to a specific table:

| Code | Source |
|------|--------|
| Joypad ports, strobe value, bit layout | Table I-7-9 / I-7-10 |
| RUN = LEFT+RIGHT chord, SELECT = UP+DOWN chord | Table I-7-9 footer |
| VSYNC at I/O 0xFDA0 | §3.6 / CaptainYS IODEF.ASM |
| CRTC index/data ports 0x440 / 0x442 | Table I-4-24 |
| Video-output mux ports 0x448 / 0x44A | Table I-4-37 |
| CRTC register file 0x00..0x1F semantics | Table I-4-20, §4.7.4 |
| Mode 11 (15 kHz 240p, 32768c) register set 14 | Table I-4-22 / I-4-23 col 14 |
| Mode 12 (31 kHz 640x480 256c) register set 1 | Table I-4-23 col 1 |
| Mode 13 (24 kHz 640x400 256c) register set 2 | Table I-4-23 col 2 |
| Mode 14 (15 kHz 720x480 256c interlace) register set 3 | Table I-4-23 col 3 |
| CR1 CLKSEL bits 1-0 (dot-clock select) | Table I-4-32 |
| CR2 PM values per dot clock | Table I-4-33 |
| ZOOM register (4x H zoom for mode 11) | Table I-4-30, §4.7.4 |
| Palette ports 0xFD90/92/94/96 (idx, B, R, G) | Table I-4-6 |
| 32768c pixel = G-R-B-555, bit 15 = transparent | Table I-4-3 + fmtowns_playground |
| VRAM bases per color depth and virtual size | Figs I-4-11..16 |
| Video-out control reg bit layout (PMODE/CL11/CL10/CL01/CL00) | Table I-4-35 |
| Video-out priority reg bit layout (PLT/YS/YM/PR1) | Table I-4-36 |
| Set 14 SIFTER (control=0x1F, priority=0x08) | Table I-4-23 footer (user-verified) |
| Set 1/2/3 SIFTER (control=0x0A, priority=0x18) | Table I-4-23 footer (user-verified) + cross-checked against fmtowns_playground |
| 32768c pixel layout = G-R-B-555 (NOT R-G-B!) | fmtowns_playground `rgb15()` macro + "paints red" demo |

### Real-Marty test results

| What we wanted to verify                            | Result |
|-----------------------------------------------------|--------|
| CD-ISO boot path                                    | **OK**: real Marty boots the same `boot/cdimage.iso` Tsugaru does, IPL + protected-mode entry + kernel jump all survive on real silicon. |
| 256-colour modes M12 (31 kHz) and M13 (24 kHz)      | **OK**: render normally; menu legible after per-mode font scaling (2x in 24/31 kHz monitor modes, 1x elsewhere). |
| 480i mode M14                                       | **OK**: works, flickers as expected for 480i. Menu at good apparent size on the TV at 1x font scale. |
| Custom 240p 256C (`CST 15K 240P 256C`)              | **OK**: mode 11's timing + 256C 1-screen layer config -- works first try, gives Marty owners a proper 240p 256-colour TV mode not in the book. |
| Custom 240p HC (`CST 15K 240P  HC`)                 | **OK**: same trick for HC -- mode 11's timing + 32768C 1-screen on Layer 1 -- works on real Marty. Confirmed that mode 11's brokenness was the 2-screen layer config, not a timing issue. Colours wrong via composite due to the Marty downscaler ASIC, same as the other HC modes. |
| Custom 31 kHz 320x240 HC (`CST 31K 320X240HC`)      | **OK**: mode 15's 640 x 480 active area + 2x H/V zoom on a 320 x 240 source, 32768C 1-screen on Layer 1. Picture fills the monitor cleanly. |
| 240p mode M11 (book set 14)                         | Still **blown up / partially visible** in its book-spec 2-screen 32768c form. Use `CST 15K 240P HC` for working 240p 32768c output. M11 is kept in the menu as the book reference and as a candidate for someone who wants to actually program both layers correctly. |
| Mode 10 (book, 2-screen, 320x240 active)            | Still **top-left quadrant only** -- two issues: 2-screen layer config plus a too-small active area. Use `CST 31K 320X240HC` instead. M10 stays in the menu as the book reference. |
| High-colour modes M15 / M16 / M17                   | **Wrong colours via composite/S-Video** -- the on-board downscaler ASIC cannot pass 15-bit RGB and truncates the signal. The CRTC almost certainly *is* outputting the right G-R-B-555 stream on its digital lines; the picture just doesn't survive the ASIC. Confirming this requires probing the digital RGBHV signals upstream of the ASIC (the motivation for the rainbow pattern). |

So as of the latest hardware pass the **CD boot pipeline, seven of
eleven modes, and the menu font scaling are all hardware-verified
working**. Remaining: three HC book modes (M15/M16/M17) are blocked
by the Marty downscaler ASIC, only verifiable from outside the
software via a pre-ASIC probe. The two book 2-screen modes (M11, M10)
remain "broken on purpose" -- the CST variants are the working path.

* **Properly program both layers for M11/M10** -- if someone wants
  the book-spec 2-screen 32768c configs to render, both Layer 0 and
  Layer 1 need to be written each frame. The patterns module would
  need a "mirror to Layer 1" pass, or the surface model would need
  to expose both apertures.
* **Run the suite on a non-Marty FM Towns** with proper RGB output
  to confirm the HC modes look right when nothing truncates the
  15-bit signal (an indirect way to corner the Marty-ASIC issue).
* **Audio.** MDFourier playback via the FM Towns sound hardware
  (YM2612 + PCM controller).
* **More patterns.** Drop-shadow / striped sprite, 1-pixel checker,
  Sonic-style scaled bricks.
* **On-screen CRTC dump.** Show the live register values so users can
  verify what the test suite actually programmed.

The grep marker for any remaining uncertain code is `TODO(hw-verify)`.
