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
correctly on each output: color bars, grid, monoscope, solid colors.

## Layout

| File         | Purpose |
|--------------|---------|
| `main.c`     | Entry point. Brings up video + input and runs the menu. |
| `video.[ch]` | CRTC programming, mode tables, palette, vsync. CPU-aware VRAM bases (`0x80000000`/`0x80100000` on 486+, `0xA00000`/`0xB00000` on 386SX/Marty). |
| `input.[ch]` | FM Towns / Marty pad reading + keyboard fallback. |
| `menu.[ch]`  | Top-level menu UI. |
| `patterns.[ch]` | Color bars / grid / monoscope / solid colors. Bpp-aware (8-bpp for 24/31 kHz 256c modes, 16-bpp G-R-B-555 for 15 kHz 240p mode). |
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

The menu exposes eight modes, four 256-colour and four 32768-colour
("high colour"). A trailing `*` in the menu label flags the modes that
have been verified working on Tsugaru-Marty; the unmarked modes have
known Tsugaru emulation quirks (collapsed interlace fields, 2-screen
compositing, 240p vertical crop) and need a real-hardware pass to
confirm whether the issue is ours or the emulator's.

| Menu label             | Book mode | Reg set | Pixel layout                                                  | Tsugaru-Marty |
|------------------------|-----------|---------|---------------------------------------------------------------|---------------|
| M11 15K 240P  HC       | mode 11   | set 14  | 128x240 logical x 16 bpp, 4x H-zoom -> ~320x240 effective TV  | crops vertically |
| M14 15K 480I 256C      | mode 14   | set 3   | 720x480 visible x 8 bpp (256c, interlaced)                    | doubles vertically (FO ignored) |
| M13 24K 640X400 *      | mode 13   | set 2   | 640x400 x 8 bpp                                               | works |
| M12 31K 640X480 *      | mode 12   | set 1   | 640x480 x 8 bpp                                               | works (menu mode) |
| M10 31K 320X240HC      | mode 10   | --      | 320x240 x 16 bpp (32768c)                                     | 2-screen compositing artefacts |
| M15 31K 320X480HC      | mode 15   | --      | 320x480 x 16 bpp (32768c)                                     | vertically squished |
| M16 15K 320X480HC      | mode 16   | --      | 320x480 x 16 bpp (32768c, 15 kHz interlace)                   | vertically doubled |
| M17 31K 512X480 *      | mode 17   | --      | 512x480 x 16 bpp (32768c, "flagship")                         | works |

The 240p mode (mode 11) is the canonical FM Towns Marty TV-out mode. It
uses both display layers configured in 32768-colour direct mode, with
Layer 0 active and 4x horizontal zoom on both layers; LO1=0x100=256
bytes/line gives 128 logical pixels per VRAM line stretched to ~320 dots.

The 32768-colour pixel format is **G-R-B-555** (bits 14-10 = green,
9-5 = red, 4-0 = blue, bit 15 reserved for the superimpose transparency
flag -- Table I-4-3, cross-checked against the `rgb15()` macro in
fmtowns_playground). `patterns.c` is bpp-aware: it writes 8-bit palette
indices in the 256-colour modes and packed 16-bit G-R-B-555 pixels in
the high-colour modes from the same drawing code.

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

What is **still unverified** and needs a real-hardware pass:

1. **Tsugaru-specific render artefacts on five of eight modes.** Modes
   10, 11, 14, 15, 16 each show a different quirk under Tsugaru-Marty
   (vertical crop, doubled fields, 2-screen compositing). The CRTC
   register values in `video.c` match the FM Towns Technical Data Book
   tables exactly, so the most likely explanation is incomplete
   emulation rather than wrong programming. This needs confirmation
   on a real Marty before we either accept the values or hunt for a bug.
2. **Real-hardware boot from the CD ISO.** Tsugaru happily boots
   `boot/cdimage.iso`, but the Marty's actual CD drive expects a
   physical disc with the same byte layout. Burning the ISO and
   booting from real hardware is the conclusive test.

The grep marker for any remaining uncertain code is `TODO(hw-verify)`.

## Future work

* **Audio.** MDFourier playback via the FM Towns sound hardware
  (YM2612 + PCM controller).
* **More patterns.** Drop-shadow / striped sprite, 1-pixel checker,
  Sonic-style scaled bricks.
* **On-screen CRTC dump.** Show the live register values so users can
  verify what the test suite actually programmed.
* **Polish the unverified modes** once we have real-hardware feedback —
  if Tsugaru is the culprit for the five quirky modes, the labels
  should lose their conditional language; if our values are wrong,
  fix them.

Please report mode-switching results on real Marty hardware (pattern
looks correct? geometry centered? TV output stable?) so the CRTC tables
can be locked in.
