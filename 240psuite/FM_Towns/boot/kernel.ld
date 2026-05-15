/*
 * Linker script for the FM Towns smoke-test kernel.
 *
 * Layout choice (matches what the IPL loads):
 *   - Image is loaded as a flat blob starting at physical 0x00010000.
 *   - First byte of the image is _start (from startup.asm).
 *   - Everything is one contiguous segment -- no separate .data/.rodata
 *     load addresses, no paging, no relocation.
 *   - .bss follows .data (zero-filled at runtime by _start).
 *
 * Total image size is bounded by main RAM minus VRAM (640 KB available);
 * we expect well under 64 KB.
 */

ENTRY(_start)

SECTIONS
{
    . = 0x00010000;

    .text : ALIGN(4) {
        /* startup MUST be first so _start is at 0x10000 */
        *(.text.startup)
        *(.text)
        *(.text.*)
    }

    .rodata : ALIGN(4) {
        *(.rodata)
        *(.rodata.*)
    }

    .data : ALIGN(4) {
        *(.data)
        *(.data.*)
    }

    .bss : ALIGN(4) {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
    }

    /* Discard everything we don't care about so they don't end up in
     * the flat binary.
     */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame)
        *(.eh_frame_hdr)
    }
}
