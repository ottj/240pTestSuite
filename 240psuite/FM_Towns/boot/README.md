# Smoke test -- "paint the screen red"

The minimal possible end-to-end test of the entire FM Towns toolchain.
If this works, the whole pipeline (boot ROM -> our IPL -> CRTC program ->
32-bit transition -> VRAM write -> visible result) is wired up correctly,
and we can start linking the real C code in.

## What it does

A 2 KB IPL sector that:

1. Programs CRTC for **mode 17** (512x480, 32768-colour, 31.47 kHz).
   Values copied verbatim from
   <https://github.com/gameblabla/fmtowns_playground/tree/simp>
   (`CRTC_SET_31` / `VIDEO_SET_31`).
2. Sets up a GDT with flat 4 GB code + data segments.
3. Switches to 32-bit protected mode.
4. Fills VRAM at `0x80100000` (512 * 480 * 2 = 491,520 bytes) with the
   16-bit pattern `0xAAAA`. In FM Towns G-R-B-555 that decodes to
   G=10, R=21, B=10 -- a reddish/pink screen.
5. Halts.

## Build and run

```
# 1. Build the floppy image.
make

# 2. Launch Tsugaru_CUI pointing at your FM Towns ROM directory.
./run.sh /path/to/rom-directory
```

`run.sh` autodetects `Tsugaru_CUI` in your `PATH`. If you built it
yourself rather than installing it, point the launcher at your own
build with `TSUGARU_CUI=/path/to/Tsugaru_CUI` -- e.g. for the macOS
`.app` bundle that the TOWNSEMU CMake build produces, that's
`<TOWNSEMU>/gui/build/main_cui/Tsugaru_CUI.app/Contents/MacOS/Tsugaru_CUI`.

## What you should see

A uniform **reddish/pink** screen at 512x480, no boot menu, no
"insert disk" prompt. The screen colour is fixed (`G=10, R=21, B=10`
in G-R-B-555) -- not pure red, more salmon -- because that's what
0xAAAA happens to decode to. We deliberately keep this byte pattern
identical to the proven gameblabla "paint red" demo so any deviation
points at *our* code, not the reference values.

## What it tells us if it doesn't work

| Symptom | Likely cause |
|---------|--------------|
| Boot device menu stays on screen | `-BOOTKEY F0` not parsed, or floppy header invalid |
| Black screen, no output at all | CRTC programming wrong, or START bit not set |
| Garbled colours / not red | RGB ordering wrong, or wrong color depth selected |
| Wrong picture geometry | CRTC HDS/HDE/VDS/VDE values wrong |
| Crash / reboot loop | Protected-mode transition broken (GDT layout, segment selectors) |

## Bytes you may need to tweak

If the screen stays black, the most likely culprit is that the CRTC's
START bit (CR0 bit 15) is being left at 0. The table in `smoke.asm`
has `CR0 = 0x000A` (matching `VIDEO_SET_31`); change it to `0x800A`
to force START = 1, and rebuild.

## Files

The boot directory contains three increasingly capable variants:

| File | Purpose |
|------|---------|
| `smoke.asm` | NASM source: minimal IPL + 32-bit PM transition + paints VRAM directly. The original first-light test. |
| `ipl_inline.asm` | IPL that embeds a C smoke kernel + jumps to it at `0x10000`. Proves the C toolchain end-to-end. |
| `ipl.asm` | Stage-1 IPL with FD sector loading (currently broken on Marty due to Tsugaru BIOS issues). |
| `cdipl.asm` | **The working full-app path**: 8 KB IPL that embeds the entire C kernel and boots from CD. |
| `smoke_main.c` | Tiny C smoke kernel for `smoke-inline`. |
| `startup.asm` | 32-bit C entry stub: sets segment selectors + stack, clears BSS, calls `main`. |
| `kernel.ld` | Linker script. Places kernel at `0x10000`. |
| `kstd.c` / `string.h` | Minimal freestanding libc replacements (`memset`, `memcpy`, etc.). |
| `io.h` | Inline-asm port I/O + keyboard polling. |
| `Makefile` | Build targets: `make smoke`, `make smoke-inline`, `make smoke-c`, `make cd`, `make` (FD). |
| `run.sh` | Launch Tsugaru_CUI. `BOOT=cd ./run.sh <rom-dir>` for CD, default is FD. |

## How to run the working app on Marty

```
make cd
BOOT=cd ./run.sh /path/to/marty-rom-dir
```

You'll get the 240p test suite menu rendered at 640x480 256-color (mode 12, 31 kHz). Navigate with arrow keys (or D-pad), press **Space/A** to enter a pattern, **Backspace** (or pad-B) to leave. **LEFT/RIGHT inside a pattern** cycles through 240p / 480i / 24 kHz / 31 kHz so you can compare the same test at every frequency.

## Marty-specific findings (vs generic FM Towns)

The 240p test suite was originally written assuming FM Towns 486+, but Marty uses an 80386SX. That changes several things; all are now handled at runtime:

| Item | 486+ FM Towns | 386SX / Marty |
|------|---------------|---------------|
| VRAM Layer 0 aperture | physical `0x80000000` | physical **`0x00A00000`** |
| VRAM Layer 1 aperture | physical `0x80100000` | physical **`0x00B00000`** |
| Machine ID port 0x30 low 2 bits | 0/1/10 | **`11` (= 3)** |
| Disk BIOS at `FFFB:0014` | works | **broken in Tsugaru** (uses CMOVBE Pentium-Pro+ instruction Tsugaru's CPU can't decode) |
| Boot from | FD or CD | **CD recommended** (FD Disk-BIOS is unusable on the firmware we tested) |

`video.c` reads I/O `0x30` at `video_init()` and picks the right VRAM base. Both flavours of FM Towns work from the same binary.

## Boot pipeline

For the CD path (`cdipl.asm`):

1. Insert CD-ROM, hit Marty's power-on. System ROM reads 4 sectors (8 KB) from LBA 0 to `B000:0000` and far-jumps to `B000:0004` with `BL = 8` (internal CD).
2. Our IPL (offset 0x5C onward) does:
   - Sets up real-mode segments.
   - Programs CRTC for an initial 31 kHz / mode 12 layout (so we have a visible screen even if the kernel hangs).
   - Copies the embedded kernel (incbin'd from `kernel-app.bin`) from `CS:offset` to physical `0x10000` via `rep movsb`.
   - Loads GDT (3 entries: null, code base=0 limit=4GB, data base=0 limit=4GB).
   - Sets `CR0.PE = 1` and far-jumps to `0x08:0x00010000`.
3. The C kernel's `_start` (in `startup.asm`):
   - Reloads DS/ES/SS/FS/GS with the data selector (`0x10`).
   - Sets ESP to `0xA0000`.
   - Zero-fills BSS.
   - Calls `main()`.
4. `main()` runs `video_init()` which detects CPU class (386SX vs 486+) and sets the right VRAM base, then `video_set_mode(HFREQ_31KHZ)` and `menu_run()`.

## Verified hardware quirks worth documenting in code

- **CRTC `LO1` register is in 8-byte units, NOT bytes.** Mode 12 with `LO1 = 0x80` gives a 1024-byte line stride. Writing pixels with `pitch = 640` shows the screen repeated 5 times.
- **Multi-byte NOPs (`0F 1F /0`) are Pentium-Pro+** and Tsugaru's CPU emulator rejects them. Compile with `-march=i386 -mtune=i386 -falign-functions=1 -falign-loops=1` to suppress.
- **Marty pad idle state may read as "all buttons pressed"** on Tsugaru, so `input_init` primes `g_prev` with the current state to avoid a spurious press-edge on the first frame (which otherwise auto-triggers the first menu item).
- **Marty has no keyboard by default**, but Tsugaru's GUI lets you press host keyboard keys that emulate FM Towns keyboard scancodes. **Backspace** maps to `PAD_B` (exit pattern). **Arrow keys** navigate. **Space** / **Enter** confirm.

## Known emulator quirks

- **480i mode (mode 14) shows the image twice vertically in Tsugaru.**
  Mode 14 is interlace per the book, with two fields offset by `FO=1024`
  bytes. Tsugaru's CRTC emulation appears to collapse both fields onto
  the same VRAM region, so the bottom half of the screen mirrors the top.
  Real Marty hardware almost certainly renders this correctly; until we
  can test on hardware (or fix Tsugaru's FO handling), use the 240p
  mode for 15 kHz output.
- **240p mode (mode 11) appears to crop more than expected vertically in
  Tsugaru.** Even a tight menu layout (9 items at 8-pixel row height
  fitting in ~88 vertical lines) gets clipped at the bottom under
  Tsugaru-Marty mode 11. Effective visible area is supposed to be 216
  scan lines per the FM Towns Technical Data Book; Tsugaru appears to
  show closer to 120. This needs testing on real Marty hardware before
  designing around it.
- **ESC may be eaten by Tsugaru's own UI.** Use **Backspace** to exit a
  pattern, or pad-B (mapped to whatever Tsugaru's `-GAMEPORT0 KEY`
  routes to B).

## Status of the FD path

`ipl.asm` (FD with sector loading) is left in the tree but not the recommended path: Marty's BIOS Disk-BIOS at `FFFB:0014` panics on this Marty firmware in Tsugaru. Direct FDC programming was attempted (`ipl.asm` has DMA + WD179X code) but Tsugaru's FDC requires DMA setup that we haven't fully figured out. CD boot bypasses all of this — Marty's native boot path anyway.
