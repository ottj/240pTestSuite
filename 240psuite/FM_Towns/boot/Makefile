#
# Build the FM Towns boot floppy.
#
# Three targets:
#   make smoke      -- ASM-only smoke test (smoke.asm).
#                      Output: floppy.bin -- proves IPL + CRTC + PM + VRAM.
#   make smoke-c    -- C smoke test through full cross-compile pipeline
#                      (ipl.asm + startup.asm + smoke_main.c).
#                      Output: floppy.bin -- proves clang/lld/objcopy chain.
#   make            -- full app: ipl + startup + real main + video.c +
#                      input.c + patterns.c + menu.c + font.c.
#                      Output: floppy.bin
#
# All targets produce a 1232 KB raw FD image at boot/floppy.bin.
#
# Requires: nasm, clang, ld.lld, llvm-objcopy. macOS clang is sufficient.
# Tested with: nasm 2.16, clang 22, ld.lld 22.1.5.

# ---- Toolchain ----------------------------------------------------------
NASM      := nasm
CC        := clang
LD        := ld.lld
OBJCOPY   := $(shell command -v llvm-objcopy 2>/dev/null || echo /opt/homebrew/opt/llvm/bin/llvm-objcopy)
DD        := dd

# clang flags for freestanding bare-metal i386.
#
# -march=i386 -mtune=i386
#     The FM Towns runs a 386/486 CPU. Tsugaru's CPU emulator doesn't
#     decode the multi-byte long-NOP (0F 1F /0) that clang otherwise
#     emits for alignment padding (Pentium-Pro-era encoding). Restricting
#     to 386 ISA forces single-byte 0x90 NOPs only.
# -falign-* 1
#     Disable padding-NOP insertion entirely, defence in depth.
CFLAGS    := -target i386-elf \
             -ffreestanding -fno-stack-protector -fno-builtin \
             -fno-pic -m32 -Os \
             -march=i386 -mtune=i386 \
             -falign-functions=1 -falign-loops=1 \
             -Wall -Wextra -Wno-unused-parameter \
             -D__FMTOWNS__ \
             -nostdinc \
             -isystem $(shell $(CC) -print-resource-dir)/include \
             -isystem . \
             -I.. -I.

NASMFLAGS_BIN := -f bin
NASMFLAGS_OBJ := -f elf32

LDFLAGS   := -m elf_i386 -nostdlib --no-undefined

# ---- Layout -------------------------------------------------------------
SRC_DIR   := ..
BOOT_DIR  := .
BUILD     := build
FD_SIZE_K := 1232          # 1232 KB 2HD floppy

# ---- Sources for the full app ------------------------------------------
APP_C_SRCS := \
    $(SRC_DIR)/main.c \
    $(SRC_DIR)/menu.c \
    $(SRC_DIR)/patterns.c \
    $(SRC_DIR)/video.c \
    $(SRC_DIR)/input.c \
    $(SRC_DIR)/font.c

APP_OBJS := $(BUILD)/startup.o $(BUILD)/kstd.o \
            $(patsubst $(SRC_DIR)/%.c,$(BUILD)/%.o,$(APP_C_SRCS))

SMOKE_C_OBJS := $(BUILD)/startup.o $(BUILD)/smoke_main.o

# ---- Top-level targets --------------------------------------------------
.PHONY: all smoke smoke-c smoke-inline cd clean

all: floppy.bin

smoke: ipl=$(BOOT_DIR)/smoke.bin
smoke: kernel_bin=
smoke: $(BUILD)/smoke.bin floppy-asm.bin
	@cp floppy-asm.bin floppy.bin
	@echo "  Built ASM-only smoke test floppy."

smoke-c: floppy-smoke-c.bin
	@cp floppy-smoke-c.bin floppy.bin
	@echo "  Built C smoke-test floppy (paint red via clang)."

smoke-inline: floppy-smoke-inline.bin
	@cp floppy-smoke-inline.bin floppy.bin
	@echo "  Built inline-kernel floppy (no FD read; isolates PM+jump)."

floppy.bin: floppy-app.bin
	@cp floppy-app.bin floppy.bin
	@echo "  Built full-app floppy."

# ---- Floppy images ------------------------------------------------------
# Each image stamps IPL into sectors 1-2 (offsets 0..2047), and a kernel
# (if any) starting at track 0 side 1 = file offset 8192.

floppy-asm.bin: $(BUILD)/smoke.bin | $(BUILD)
	$(DD) if=/dev/zero of=$@ bs=1024 count=$(FD_SIZE_K) status=none
	$(DD) if=$(BUILD)/smoke.bin of=$@ conv=notrunc status=none

floppy-smoke-c.bin: $(BUILD)/ipl.bin $(BUILD)/kernel-smoke.bin | $(BUILD)
	$(DD) if=/dev/zero of=$@ bs=1024 count=$(FD_SIZE_K) status=none
	$(DD) if=$(BUILD)/ipl.bin            of=$@ conv=notrunc bs=1 count=2048 status=none
	$(DD) if=$(BUILD)/kernel-smoke.bin   of=$@ conv=notrunc bs=1 seek=8192 status=none
	@echo "  floppy-smoke-c.bin: IPL(2KB) + kernel-smoke ($$(stat -f%z $(BUILD)/kernel-smoke.bin) bytes) at +8KB"

cd: cdimage.iso
	@echo "  Built CD ISO at cdimage.iso (boot with -CD <path> -BOOTKEY CD)."

# CD ISO: 8 KB IPL+kernel block at start, padded to 1 MB. Tsugaru accepts
# this raw format directly via -CD. No ISO 9660 filesystem needed -- the
# system ROM only cares about the boot sector layout.
$(BUILD)/cdipl.bin: cdipl.asm $(BUILD)/kernel-app.bin | $(BUILD)
	$(NASM) $(NASMFLAGS_BIN) -o $@ $<
	@sz=$$(stat -f%z $@); \
	  if [ "$$sz" != "8192" ]; then \
	    echo "ERROR: cdipl.bin is $$sz bytes, expected 8192" >&2; exit 1; \
	  fi

cdimage.iso: $(BUILD)/cdipl.bin | $(BUILD)
	$(DD) if=/dev/zero of=$@ bs=2048 count=512 status=none
	$(DD) if=$(BUILD)/cdipl.bin of=$@ conv=notrunc bs=1 count=8192 status=none
	@echo "  cdimage.iso: 8 KB IPL+kernel block + zero pad to 1 MB."

floppy-app.bin: $(BUILD)/ipl.bin $(BUILD)/kernel-app.bin | $(BUILD)
	$(DD) if=/dev/zero of=$@ bs=1024 count=$(FD_SIZE_K) status=none
	$(DD) if=$(BUILD)/ipl.bin            of=$@ conv=notrunc bs=1 count=2048 status=none
	$(DD) if=$(BUILD)/kernel-app.bin     of=$@ conv=notrunc bs=1 seek=8192 status=none
	@sz=$$(stat -f%z $(BUILD)/kernel-app.bin); \
	  if [ $$sz -gt 32768 ]; then \
	    echo "ERROR: kernel-app.bin is $$sz bytes -- exceeds 32 KB IPL load window." >&2; \
	    exit 1; \
	  fi; \
	  echo "  floppy-app.bin: IPL(2KB) + kernel-app ($$sz bytes) at +8KB"

# ---- IPL ----------------------------------------------------------------
$(BUILD)/smoke.bin: smoke.asm | $(BUILD)
	$(NASM) $(NASMFLAGS_BIN) -o $@ $<
	@sz=$$(stat -f%z $@); \
	  if [ "$$sz" != "2048" ]; then \
	    echo "ERROR: smoke.bin is $$sz bytes, expected 2048" >&2; exit 1; \
	  fi

$(BUILD)/ipl.bin: ipl.asm | $(BUILD)
	$(NASM) $(NASMFLAGS_BIN) -o $@ $<
	@sz=$$(stat -f%z $@); \
	  if [ "$$sz" != "2048" ]; then \
	    echo "ERROR: ipl.bin is $$sz bytes, expected 2048" >&2; exit 1; \
	  fi

# Inline-kernel IPL: 2 KB that embeds the C smoke kernel via `incbin`.
# Depends on kernel-smoke.bin being built first.
$(BUILD)/ipl_inline.bin: ipl_inline.asm $(BUILD)/kernel-smoke.bin | $(BUILD)
	$(NASM) $(NASMFLAGS_BIN) -o $@ $<
	@sz=$$(stat -f%z $@); \
	  if [ "$$sz" != "2048" ]; then \
	    echo "ERROR: ipl_inline.bin is $$sz bytes, expected 2048" >&2; exit 1; \
	  fi

floppy-smoke-inline.bin: $(BUILD)/ipl_inline.bin | $(BUILD)
	$(DD) if=/dev/zero of=$@ bs=1024 count=$(FD_SIZE_K) status=none
	$(DD) if=$(BUILD)/ipl_inline.bin of=$@ conv=notrunc bs=1 count=2048 status=none
	@echo "  floppy-smoke-inline.bin: IPL+embedded kernel in first 2 KB."

# ---- Kernel objects -----------------------------------------------------
$(BUILD)/startup.o: startup.asm | $(BUILD)
	$(NASM) $(NASMFLAGS_OBJ) -o $@ $<

$(BUILD)/smoke_main.o: smoke_main.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kstd.o: kstd.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# ---- Linking + flatten --------------------------------------------------
$(BUILD)/kernel-smoke.elf: $(SMOKE_C_OBJS) kernel.ld
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $(SMOKE_C_OBJS)

$(BUILD)/kernel-smoke.bin: $(BUILD)/kernel-smoke.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/kernel-app.elf: $(APP_OBJS) kernel.ld
	$(LD) $(LDFLAGS) -T kernel.ld -o $@ $(APP_OBJS)

$(BUILD)/kernel-app.bin: $(BUILD)/kernel-app.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD):
	@mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD) floppy.bin floppy-asm.bin floppy-smoke-c.bin floppy-smoke-inline.bin floppy-app.bin
