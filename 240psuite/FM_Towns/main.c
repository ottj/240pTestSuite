/*
 * 240p Test Suite - FM Towns port
 * Copyright (C) 2026 Artemio Urbina
 *
 * GPLv2 or later -- see LICENSE in repo root.
 */

#include "video.h"
#include "input.h"
#include "menu.h"

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    input_init();
    video_init();

    /* Boot to 31 kHz 640x480 256c -- this is the mode the menu and font
     * code are designed for (8-bit packed pixels into a hardware palette).
     * The user can switch to 240p / 480i / 24kHz from the main menu
     * after they pick a test pattern; the patterns themselves handle the
     * 16-bpp 32768-color path for 240p.
     */
    video_set_mode(HFREQ_31KHZ);

    menu_run();

    video_shutdown();
    return 0;
}
