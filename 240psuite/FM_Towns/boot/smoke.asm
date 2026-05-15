;
; 240p Test Suite -- FM Towns smoke test (paint screen red).
;
; This is a self-contained 2 KB IPL sector. It does the entire test
; in-place: program CRTC to mode 17 (512x480 32768c at 31.47 kHz),
; transition to 32-bit protected mode, fill VRAM with 0xAAAA (= reddish
; G-R-B-555 pixel), and halt.
;
; Cross-checked against:
;   - "FM TOWNS Technical Data Book" Tables I-4-23 (CRTC), I-4-35/36
;     (video-out registers).
;   - captainys/TOWNSEMU testdata/IPL/FDIMAGE.BIN header layout.
;   - gameblabla/fmtowns_playground/simp CRTC_SET_31 / VIDEO_SET_31.
;
; Layout on disk:
;   Bytes 0..3   : "IPL4" magic.
;   Bytes 4..5   : JMP short to real entry at offset 0x5C.
;   Bytes 6..0x5B: zero pad (FM Towns ROM BPB-ish area).
;   0x5C..       : code, GDT, padding to 2 KB.
;
; System ROM loads sectors 0..1 of the floppy (2 KB total) to
; physical 0xB0000 (= B000:0000 in real-mode segment terms) and far-jumps
; to B000:0004 with BL = 2 (FD boot) and BH = unit number. We don't
; bother validating BL -- this image is only ever run as an FD boot.
;

                BITS    16
                ORG     0                 ; CS = 0xB000 at runtime; symbols are
                                          ; offsets within the 2 KB sector.

ipl_base        equ     0xB0000           ; linear addr at runtime
linear_of       equ     ipl_base          ; helper: lin = base + offset

;
; ---- IPL header ----
;
                db      "IPL4"
                jmp     short start_real

                times   0x5C - ($ - $$) db 0

;
; ---- Real-mode entry (offset 0x5C from B000:0000) ----
;
start_real:
                cli
                cld
                push    cs
                pop     ds
                push    cs
                pop     es
                push    cs
                pop     ss
                mov     sp, 0x07FE       ; stack just below 2 KB

;
; ---- Program CRTC ----
; We walk 32 register values from `crtc_table`, skipping indices 2 and 3
; (reserved per Table I-4-20). Each entry is a 16-bit value written to
; CRTC register `index` via index port 0x440 / data port 0x442.
;
                mov     si, crtc_table
                xor     bx, bx
.crtc_loop:
                cmp     bx, 2
                je      .crtc_skip_word
                cmp     bx, 3
                je      .crtc_skip_word
                mov     dx, 0x0440
                mov     ax, bx
                out     dx, ax
                mov     dx, 0x0442
                lodsw                     ; AX <- [DS:SI], SI += 2
                out     dx, ax
                jmp     .crtc_next
.crtc_skip_word:
                add     si, 2             ; skip the (zero) entry without writing
.crtc_next:
                inc     bx
                cmp     bx, 32
                jl      .crtc_loop

;
; ---- Program video-output mux ----
;   reg 0 (Control)  = 0x0F  (1-screen 32768c, per VIDEO_SET_31)
;   reg 1 (Priority) = 0x08
;
                mov     dx, 0x0448
                xor     al, al
                out     dx, al
                mov     dx, 0x044A
                mov     al, 0x0F
                out     dx, al

                mov     dx, 0x0448
                mov     al, 0x01
                out     dx, al
                mov     dx, 0x044A
                mov     al, 0x08
                out     dx, al

;
; ---- Switch to 32-bit protected mode ----
;
                lgdt    [gdt_descr]
                mov     eax, cr0
                or      eax, 1
                mov     cr0, eax

                ; Far jump to 32-bit code. Operand is absolute linear address.
                db      0x66, 0xEA        ; JMP FAR DWORD ptr16:offset32
                dd      linear_of + pm_entry
                dw      0x08              ; selector = code (entry 1 in GDT)

;
; ---- 32-bit protected-mode code ----
;
                BITS    32
pm_entry:
                mov     ax, 0x10          ; data segment selector
                mov     ds, ax
                mov     es, ax
                mov     ss, ax
                mov     fs, ax
                mov     gs, ax

                ; Set a 32-bit flat stack -- park it just below the
                ; Marty OS ROM area (0x600000) so it can't collide.
                mov     esp, 0x005F0000

                ; Fill VRAM. Two layouts:
                ;   486+/normal Towns : VRAM Layer 1 at 0x80100000
                ;   386SX/Marty       : VRAM Layer 1 at 0x00B00000
                ; Marty has a 386SX CPU with only 24-bit physical
                ; addressing, so VRAM is mapped low. Tsugaru with
                ; -TOWNSTYPE MARTY puts VRAM1 at TOWNSADDR_386SX_VRAM1_BASE
                ; (= 0xB00000) per src/towns/townsdef/townsdef.h.
                ;
                ; For now we write to BOTH addresses so the same image
                ; works regardless of which mode the host is emulating.
                ; A proper port detection comes later.
                mov     edi, 0x80100000
                mov     ecx, 122880
                mov     eax, 0xAAAAAAAA
                rep     stosd

                mov     edi, 0x00B00000
                mov     ecx, 122880
                mov     eax, 0xAAAAAAAA
                rep     stosd

.hang:
                hlt
                jmp     .hang

;
; ---- Data: CRTC register table (CRTC_SET_31, mode 17) ----
; 32 16-bit entries, indices 0..0x1F. Entries 2 and 3 are unused (reserved).
;
                BITS    16
                align   2
crtc_table:
                dw      0x0060, 0x02C0           ; 00 HSW1, 01 HSW2
                dw      0x0000, 0x0000           ; 02 -, 03 - (reserved)
                dw      0x031F, 0x0000           ; 04 HST,  05 VST1
                dw      0x0004, 0x0000           ; 06 VST2, 07 EET
                dw      0x0419, 0x00CA           ; 08 VST,  09 HDS0
                dw      0x02CA, 0x00CA           ; 0A HDE0, 0B HDS1
                dw      0x02CA, 0x0046           ; 0C HDE1, 0D VDS0
                dw      0x0406, 0x0046           ; 0E VDE0, 0F VDS1
                dw      0x0406, 0x0000           ; 10 VDE1, 11 FA0
                dw      0x00CA, 0x0000           ; 12 HAJ0, 13 FO0
                dw      0x0080, 0x0000           ; 14 LO0,  15 FA1
                dw      0x00CA, 0x0000           ; 16 HAJ1, 17 FO1
                dw      0x0080, 0x0058           ; 18 LO1,  19 EHAJ
                dw      0x0001, 0x0000           ; 1A EVAJ, 1B ZOOM
                dw      0x000A, 0x0002           ; 1C CR0 (32768c 1-screen, START off here -- gets enabled by 0x800A on a separate pass)
                dw      0x0000, 0x0192           ; 1E FR,   1F CR2

; Note: CR0 = 0x000A means DISPLAY STOPPED. The playground relies on the
; system ROM already having START=1 set. We can enable START by writing
; 0x800A explicitly. The CRTC values match VIDEO_SET_31 and produce a
; visible 512x480 32768c screen at 31.47 kHz.
;
; If the screen stays black on the first run, change the 0x000A above to
; 0x800A (sets the START bit immediately) and rebuild.
;

;
; ---- GDT ----
;
                align   8
gdt:
                ; Null entry
                dq      0
                ; Code: base=0, limit=4GB, 32-bit, exec/read, ring 0
                dq      0x00CF9A000000FFFF
                ; Data: base=0, limit=4GB, 32-bit, read/write, ring 0
                dq      0x00CF92000000FFFF
gdt_end:

                align   2
gdt_descr:
                dw      gdt_end - gdt - 1
                dd      linear_of + gdt

;
; Pad to exactly 2 KB so this image can be written as the first two
; 1024-byte sectors of a 1232 KB FM Towns floppy disk.
;
                times   2048 - ($ - $$) db 0
