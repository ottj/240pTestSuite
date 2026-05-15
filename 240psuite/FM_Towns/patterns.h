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
 *   - press LEFT/RIGHT to cycle through the visible video modes
 *     (the same set the menu shows, in the same order, wrapping at
 *     the ends), so they can compare the same pattern across modes
 *     without going back to the menu. The broken book modes M11 and
 *     M10 are skipped from this cycle the same way they're absent
 *     from the menu.
 *   - press A/RUN if the pattern has additional sub-states (e.g.
 *     toggling grid color).
 */
void pattern_color_bars(void);
void pattern_grid(void);
void pattern_monoscope(void);
void pattern_solid_colors(void);
void pattern_rainbow(void);

#endif
