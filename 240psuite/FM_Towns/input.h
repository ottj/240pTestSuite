/*
 * 240p Test Suite - FM Towns port
 * Copyright (C) 2026 Artemio Urbina
 *
 * GPLv2 or later -- see LICENSE in repo root.
 */

#ifndef __FMT_INPUT_H
#define __FMT_INPUT_H

#include "types.h"

/* Logical buttons. The FM Towns / Marty pad maps to these directly.
 * The keyboard (when available) is a fallback: arrows, Z=A, X=B,
 * Return=RUN, Space=SELECT, ESC=quit.
 *
 * UP/DOWN/LEFT/RIGHT navigate menus; A confirms; B goes back;
 * RUN doubles as "confirm" (the FM Towns Marty doesn't have an
 * obvious "OK" button so we accept either A or RUN).
 */
#define PAD_UP      0x0001
#define PAD_DOWN    0x0002
#define PAD_LEFT    0x0004
#define PAD_RIGHT   0x0008
#define PAD_A       0x0010
#define PAD_B       0x0020
#define PAD_RUN     0x0040   /* "START"  */
#define PAD_SELECT  0x0080
#define PAD_QUIT    0x8000   /* keyboard ESC -- only available with a kb */

#define PAD_CONFIRM (PAD_A | PAD_RUN)
#define PAD_CANCEL  (PAD_B | PAD_SELECT)

/* One-time setup. */
void input_init(void);

/* Call once per frame. Updates internal "current" and "edge" state.
 * Must be called after video_wait_vblank() for cleanest input timing.
 */
void input_poll(void);

/* Bitmask of buttons currently held down. */
u16  input_held(void);

/* Bitmask of buttons that became pressed THIS frame (rising edge). */
u16  input_pressed(void);

#endif
