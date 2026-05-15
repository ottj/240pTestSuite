/*
 * Stage-2 smoke test in C.
 *
 * Same end state as boot/smoke.asm (paint VRAM reddish), but reached
 * through the full clang -target i386-elf + ld.lld toolchain. Success
 * here proves the cross-compile / link / load / jump path; we can then
 * swap this for the real main.c with high confidence in the runtime.
 *
 * Linked at 0x00010000 by kernel.ld. The IPL has already programmed
 * the CRTC to mode 17 (512x480 32768c), so we just write the pixels.
 */

#include <stdint.h>

/*
 * VRAM Layer 1 (where mode 17 reads from) is at different physical
 * addresses depending on the CPU:
 *   - 486+ Towns: 0x80100000 (per FM Towns Technical Data Book Fig I-4-14)
 *   - 386SX Towns / Marty: 0x00B00000 (Tsugaru's TOWNSADDR_386SX_VRAM1_BASE,
 *     which reflects the fact that 386SX has only 24-bit physical addressing
 *     so all VRAM apertures live below 16 MB).
 *
 * Detection by machine-ID I/O port 0x30. The low 2 bits encode the CPU
 * type (per Tsugaru FMTownsCommon::MachineID + FM Towns Technical Data
 * Book pp.775/781):
 *   00 = 80286
 *   01 = 80386DX
 *   10 = 80486SX / 80486DX / Pentium
 *   11 = 80386SX  <-- Marty and Towns 2 UX
 *
 * On 386SX machines VRAM is mapped low (0xA00000 / 0xB00000) because
 * the CPU's physical address space is 24-bit. On 486+ machines VRAM
 * sits at 0x80000000 / 0x80100000.
 */

static inline uint8_t inb(uint16_t port)
{
    uint8_t v;
    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

int main(void)
{
    volatile uint32_t *lowprobe = (volatile uint32_t *)0x00000600;
    uint8_t machine_id_low = inb(0x0030);
    uint32_t vram_addr =
        ((machine_id_low & 0x03) == 0x03) ? 0x00B00000u   /* 386SX Towns/Marty */
                                          : 0x80100000u;   /* 386DX / 486+ Towns */
    volatile uint16_t *vram = (volatile uint16_t *)vram_addr;
    int i;

    /* Diagnostic: low-memory probe + record the machine-ID byte we read. */
    lowprobe[0] = 0xDEADBEEF;
    lowprobe[1] = 0xCAFE0000u | machine_id_low;
    lowprobe[2] = vram_addr;

    /* Top half white (0x7FFF), bottom half blue (0x001F) -- G-R-B-555. */
    for (i = 0; i < 512 * 240; ++i) vram[i] = 0x7FFF;
    for (     ; i < 512 * 480; ++i) vram[i] = 0x001F;

    for (;;) __asm__ __volatile__("hlt");
    return 0;
}
