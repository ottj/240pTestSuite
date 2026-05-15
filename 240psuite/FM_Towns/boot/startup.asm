;
; 32-bit kernel entry stub.
;
; The IPL has already:
;   - Programmed the CRTC (initial mode -- main() can change it).
;   - Loaded GDT, set CR0.PE, far-jumped here with CS = code-segment
;     selector and DS/ES/SS/FS/GS = data-segment selector.
;   - Pointed ESP at a usable stack somewhere below VRAM.
;
; Our only jobs before C runs:
;   1. Zero the .bss section (so static/global variables start at 0).
;   2. Call main().
;   3. If main() ever returns, halt.
;
; The linker script defines `_bss_start` and `_bss_end` for us.
;

                BITS    32
                section .text.startup

                extern  main
                extern  _bss_start
                extern  _bss_end

                global  _start
_start:
                ; CRITICAL: at PM entry the segment regs other than CS
                ; still hold the real-mode values from the IPL (DS=ES=SS
                ; =FS=GS = 0xB000). In PM those are bogus selectors and
                ; the first memory access will #GP -> #DF -> triple-fault
                ; reset. Set them all to the data selector (GDT index 2)
                ; that the IPL set up before its far jump here.
                mov     ax, 0x10
                mov     ds, ax
                mov     es, ax
                mov     ss, ax
                mov     fs, ax
                mov     gs, ax

                ; Stack: park it well below VRAM and well above our
                ; kernel image. 0xA0000 (640 KB mark) is the classic safe
                ; spot in flat real-mode-friendly RAM.
                mov     esp, 0x000A0000

                cld

                ; Zero BSS, dword at a time. _bss_start and _bss_end are
                ; aligned to 4 by the linker script.
                mov     edi, _bss_start
                mov     ecx, _bss_end
                sub     ecx, edi
                shr     ecx, 2
                xor     eax, eax
                rep     stosd

                ; Hand off to C.
                call    main

                ; main() shouldn't return in normal operation; if it does,
                ; halt forever rather than fall through into garbage.
.hang:
                hlt
                jmp     .hang
