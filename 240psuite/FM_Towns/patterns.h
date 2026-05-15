/*
 * 240p Test Suite - FM Towns port
 * Copyright (C) 2026 Artemio Urbina
 *
 * GPLv2 or later -- see LICENSE in repo root.
 */

#ifndef __FMT_PATTERNS_H
#define __FMT_PATTERNS_H

/* Each pattern runs its own little loop. It returns when the user
 * presses CANCEL (B / SELECT / ESC). Inside the pattern the user can:
 *   - press LEFT/RIGHT to cycle the horizontal frequency
 *     (15 -> 24 -> 31 kHz, wrapping), so they can compare the same
 *     pattern across all three outputs without going back to the menu.
 *   - press A/RUN if the pattern has additional sub-states (e.g.
 *     toggling grid color).
 */
void pattern_color_bars(void);
void pattern_grid(void);
void pattern_monoscope(void);
void pattern_solid_colors(void);

#endif
