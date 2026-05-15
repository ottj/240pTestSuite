;
; Diagnostic IPL: skip FD read entirely. The C kernel is embedded inline
; via `incbin` and copied from CS:offset to 0x1000:0x0000 in real mode,
; then we do the normal CRTC+PM+jump.
;
; Purpose: if this paints red, the bug is specifically in the FD-read
; path. If it doesn't, the bug is somewhere in PM transition or the jump
; to 0x10000.
;
; The kernel must be small enough that the IPL header (0x60) + copy code
; (~50 bytes) + CRTC code (~60 bytes) + GDT (24) + kernel + tail still
; fits in 2 KB. C smoke kernel is ~131 bytes so we have plenty of room.
;

                BITS    16
                ORG     0
ipl_base        equ     0xB0000
linear_of       equ     ipl_base

                db      "IPL4"
                jmp     short start_real
                times   0x5C - ($-$$) db 0

start_real:
                cli
                cld
                push    cs
                pop     ds
                push    cs
                pop     es
                push    cs
                pop     ss
                mov     sp, 0x07FE

                ; --- Program CRTC for mode 17 ---
                mov     si, crtc_table
                xor     bx, bx
.crtc_loop:
                cmp     bx, 2
                je      .crtc_skip
                cmp     bx, 3
                je      .crtc_skip
                mov     dx, 0x0440
                mov     ax, bx
                out     dx, ax
                mov     dx, 0x0442
                lodsw
                out     dx, ax
                jmp     .crtc_next
.crtc_skip:
                add     si, 2
.crtc_next:
                inc     bx
                cmp     bx, 32
                jl      .crtc_loop

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

                ; --- Copy inline kernel from CS:embedded_kernel to 0x1000:0 ---
                push    ds
                mov     si, embedded_kernel
                mov     ax, 0x1000
                mov     es, ax
                xor     di, di
                mov     cx, kernel_end - embedded_kernel
                rep     movsb
                pop     ds

                ; --- PM transition ---
                lgdt    [gdt_descr]
                mov     eax, cr0
                or      eax, 1
                mov     cr0, eax

                db      0x66, 0xEA
                dd      0x00010000
                dw      0x08

;
; Data
;
                align   2
crtc_table:
                dw      0x0060, 0x02C0
                dw      0x0000, 0x0000
                dw      0x031F, 0x0000
                dw      0x0004, 0x0000
                dw      0x0419, 0x00CA
                dw      0x02CA, 0x00CA
                dw      0x02CA, 0x0046
                dw      0x0406, 0x0046
                dw      0x0406, 0x0000
                dw      0x00CA, 0x0000
                dw      0x0080, 0x0000
                dw      0x00CA, 0x0000
                dw      0x0080, 0x0058
                dw      0x0001, 0x0000
                dw      0x800A, 0x0002
                dw      0x0000, 0x0192

                align   8
gdt:
                dq      0
                dq      0x00CF9A000000FFFF
                dq      0x00CF92000000FFFF
gdt_end:

                align   2
gdt_descr:
                dw      gdt_end - gdt - 1
                dd      linear_of + gdt

                align   4
embedded_kernel:
                incbin  "build/kernel-smoke.bin"
kernel_end:

                times   2048 - ($-$$) db 0
