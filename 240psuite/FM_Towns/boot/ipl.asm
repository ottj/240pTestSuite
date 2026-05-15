;
; 240p Test Suite -- FM Towns Stage-1 IPL.
;
; Sector 0..1 of the boot floppy. Loaded by the system ROM to B000:0000
; on FD boot. Entry at B000:0004 with BL = 2 (FD), BH = unit number.
;
; Responsibilities:
;   1. Read 32 KB of kernel from the FD into physical 0x00010000 via
;      the Disk BIOS at FFFB:0014.
;   2. Program CRTC + video-out for an initial visible mode (mode 17,
;      512x480 32768c at 31.47 kHz -- same as smoke.asm so first-light
;      shows whatever the kernel paints).
;   3. Load GDT, enter 32-bit protected mode, far-jump to 0x10000.
;
; Floppy layout produced by Makefile:
;   Track 0, side 0, sectors 1..2 : this IPL  (2 KB)
;   Track 0, side 0, sectors 3..8 : padding   (6 KB unused)
;   Track 0, side 1, sectors 1..8 : kernel    (8 KB)
;   Track 1, side 0, sectors 1..8 : kernel    (8 KB)
;   Track 1, side 1, sectors 1..8 : kernel    (8 KB)
;   Track 2, side 0, sectors 1..8 : kernel    (8 KB)
;   --> 32 KB of kernel area, plenty for the smoke test + full menu.
;
; Disk BIOS call (AH=05h Read Sector) per CaptainYS FDIPLBAS.ASM:
;   AL = device id (0x20 | (unit & 7))
;   CH = 0, CL = track
;   DH = side, DL = starting sector (1-based)
;   BL = sectors to read on this track, BH = 0
;   ES:DI = destination buffer
;

                BITS    16
                ORG     0
ipl_base        equ     0xB0000
linear_of       equ     ipl_base

;
; ---- IPL header ----
;
                db      "IPL4"
                jmp     short start_real
                times   0x5C - ($-$$) db 0

;
; ---- Stage 1 code (offset 0x5C) ----
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
                mov     sp, 0x07FE

                ; Save the device-info bytes from the system ROM so we can
                ; reference them later. BL=2 for FD, BH=unit number.
                mov     [boot_bl], bl
                mov     [boot_bh], bh

;
; ---- Program CRTC FIRST ------------------------------------------------
; Doing this before sector reads guarantees we always end up in a known
; visible mode (mode 17, 512x480 32768c). Even if the FD load later fails
; and we paint a debug pattern instead of running the kernel, the user can
; see the IPL ran (the screen is in the right mode, no boot-device menu).
;
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

;
; ---- Read kernel: 32 KB into physical 0x10000 via FDC + DMA ----
;
; Tsugaru's FDC emulation always uses DMA (no PIO data path), so we
; have to program the DMA controller (channel 0 = FDC) for each chunk.
;
; Floppy layout: 1232 KB 2HD, 8 sectors of 1024 bytes per side.
;     cyl 0 side 1 -> kernel +0..+8 KB    -> physical 0x00010000
;     cyl 1 side 0 -> kernel +8..+16 KB   -> physical 0x00012000
;     cyl 1 side 1 -> kernel +16..+24 KB  -> physical 0x00014000
;     cyl 2 side 0 -> kernel +24..+32 KB  -> physical 0x00016000
;
; FDC ports (WD179X-compatible, Tsugaru fdc.cpp):
;     0x200  status (R) / command (W)
;     0x202  track register
;     0x204  sector register
;     0x205  data register (we only use it for the SEEK target track)
;     0x208  control: bit 0=IRQMSK 1=DDEN 2=SIDE 4=MOTOR
;     0x20C  drive select (bit 0..3)
;
; DMA controller (uPD71071-style, channel 0 = FDC) at 0xA0..0xAF.
;
                ; Drive select + motor on, side 1, MFM
                mov     al, 0x01
                mov     dx, 0x020C
                out     dx, al
                mov     al, 0x15         ; SIDE=1 MOTOR=1 IRQMSK=1
                mov     dx, 0x0208
                out     dx, al

                ; Restore to track 0
                mov     al, 0x03
                mov     dx, 0x0200
                out     dx, al
                call    fdc_wait_not_busy

                ; Loop 4 chunks of 8 KB each
                mov     word [ts_enc], 1   ; cyl 0, side 1
                mov     dword [dma_addr], 0x00010000
                mov     cx, 4
.chunk_loop:
                push    cx

                ; --- Set up DMA channel 0 -----------------------------------
                ; Mask channel 0 while we reprogram it
                mov     al, 0x01
                mov     dx, 0x00AF
                out     dx, al

                ; Init: reset state, 8-bit bus (bit 1 clear)
                mov     al, 0x01
                mov     dx, 0x00A0
                out     dx, al

                ; Select channel 0, BASE=0 (writes go to current regs)
                mov     al, 0x00
                mov     dx, 0x00A1
                out     dx, al

                ; Count = 8192 - 1 = 0x1FFF
                mov     ax, 0x1FFF
                mov     dx, 0x00A2
                out     dx, al
                mov     al, ah
                mov     dx, 0x00A3
                out     dx, al

                ; Address (32-bit physical)
                mov     eax, [dma_addr]
                mov     dx, 0x00A4
                out     dx, al
                shr     eax, 8
                mov     dx, 0x00A5
                out     dx, al
                shr     eax, 8
                mov     dx, 0x00A6
                out     dx, al
                shr     eax, 8
                mov     dx, 0x00A7
                out     dx, al

                ; Mode: single transfer, IO read (device->mem), 8-bit, inc
                ; uPD71071 mode register:
                ;   bits 7   = ADIR (0=increment)
                ;   bit  6   = AUTI (0=no autoinit)
                ;   bits 5:4 = DTRT direction (01=IO read = device->memory)
                ;   bits 3:2 = TMOD (01=single)
                ;   bit  0   = TS (0=byte transfers)
                mov     al, 0x14          ; 0001_0100 = IO-read, single, byte
                mov     dx, 0x00AA
                out     dx, al

                ; Unmask channel 0 (mask all others)
                mov     al, 0x0E
                mov     dx, 0x00AF
                out     dx, al

                ; --- FDC: side, seek, read ----------------------------------
                ; Decode side / cyl from ts_enc
                mov     bx, [ts_enc]
                mov     ah, 0
                test    bx, 1
                jz      .side0
                mov     ah, 0x04
.side0:
                shr     bx, 1
                ; BL = cyl, AH = SIDE bit for 0x208

                mov     al, ah
                or      al, 0x11          ; MOTOR=1, IRQMSK=1
                mov     dx, 0x0208
                out     dx, al

                ; Seek to BL
                mov     al, bl
                mov     dx, 0x0205
                out     dx, al            ; data reg = target track
                mov     al, 0x14          ; Seek, verify, 15ms
                mov     dx, 0x0200
                out     dx, al
                call    fdc_wait_not_busy

                ; Start sector = 1
                mov     al, 1
                mov     dx, 0x0204
                out     dx, al

                ; Read multi-sector (0x88 = read, multi, no side compare,
                ; 15ms settle).
                mov     al, 0x88
                mov     dx, 0x0200
                out     dx, al

                ; The FDC will tell DMA to transfer all 8192 bytes; just
                ; wait for BUSY to clear.
                call    fdc_wait_not_busy

                ; Advance to next chunk (use dec/jnz near because `loop`
                ; only takes a short displacement and the chunk body is now
                ; bigger than 127 bytes).
                inc     word [ts_enc]
                add     dword [dma_addr], 8192
                pop     cx
                dec     cx
                jnz     near .chunk_loop

                ; Motor off
                mov     al, 0x01
                mov     dx, 0x0208
                out     dx, al

;
; ---- Enter 32-bit protected mode, jump to kernel ----
;
                lgdt    [gdt_descr]
                mov     eax, cr0
                or      eax, 1
                mov     cr0, eax

                ; Far jump to 0x08:0x00010000 (kernel _start).
                db      0x66, 0xEA
                dd      0x00010000
                dw      0x08

;
; ---- Error handlers ----
;
.read_error:
                cli
.error_hang:
                hlt
                jmp     .error_hang

;
; ---- fdc_wait_not_busy -------------------------------------------------
; Spin on FDC status (port 0x200) until bit 0 (BUSY) clears.
; Trashes AX, DX. Preserves everything else.
;
fdc_wait_not_busy:
                push    dx
                mov     dx, 0x0200
.wnb:
                in      al, dx
                test    al, 0x01
                jnz     .wnb
                pop     dx
                ret

;
; ---- Data ----
;
boot_bl         db      0
boot_bh         db      0
ts_enc          dw      0
                align   4
dma_addr        dd      0

                align   2
crtc_table:
                ; Mode 17 (512x480 32768c, 31.47 kHz). Same values as smoke.asm.
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
                dw      0x800A, 0x0002   ; CR0 has START bit forced
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

                times   2048 - ($-$$) db 0
