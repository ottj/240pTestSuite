;
; CD-boot IPL: combines IPL header + 32-bit PM transition + the C kernel
; (embedded via incbin) into a single 8 KB block.
;
; FM Towns CD boot:
;   - System ROM reads 4 sectors (= 8 KB) of the CD into B000:0000.
;   - Entry at B000:0004 with BL=1 (SCSI CD) or BL=8 (internal CD).
;   - This avoids needing any further disk I/O (Marty's BIOS Disk-BIOS
;     calls crash inside Tsugaru), which is the whole reason for moving
;     from the FD path to the CD path.
;
; Our kernel binary is ~7.8 KB; together with this IPL stub it fits in
; the 8 KB auto-loaded window. We just copy the kernel from B000:offset
; to physical 0x10000 in real mode, then do the usual PM transition.
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
                mov     sp, 0x07FE        ; stack within the IPL area

                ; Accept both internal CD (BL=8) and SCSI CD (BL=1).
                cmp     bl, 8
                je      .is_cd
                cmp     bl, 1
                je      .is_cd
                retf
.is_cd:

                ; --- Program CRTC for mode 17 ------------------------------
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

                ; --- Copy embedded kernel to physical 0x10000 --------------
                mov     si, embedded_kernel
                mov     ax, 0x1000
                mov     es, ax
                xor     di, di
                mov     cx, kernel_end - embedded_kernel
                rep     movsb

                ; --- Enter 32-bit protected mode ----------------------------
                lgdt    [gdt_descr]
                mov     eax, cr0
                or      eax, 1
                mov     cr0, eax

                db      0x66, 0xEA
                dd      0x00010000
                dw      0x08

;
; ---- Data: CRTC table (mode 17, 32768c, 31 kHz) ------------------------
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

                ; Kernel embedded at end of IPL data. The entry sits
                ; somewhere in the B000-segment region; we copy it to
                ; 0x10000 (its link address) before the PM jump.
                align   4
embedded_kernel:
                incbin  "build/kernel-app.bin"
kernel_end:

                ; Pad to 8 KB (4 CD sectors of 2048 bytes each).
                times   8192 - ($-$$) db 0
