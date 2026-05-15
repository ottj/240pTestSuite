# 240p Test Suite -- FM Towns port
#
# Builds with the Townsdev / TownsAPI toolchain (i386 cross-GCC targeting
# FM Towns EXP executables). Adjust TOWNSDEV to your installation root.
#
# Example:
#   make TOWNSDEV=/opt/townsdev
#
# Output: bin/240PFMT.EXP  +  release/240PFMT.EXP

TOWNSDEV ?= /opt/townsdev
PREFIX   ?= $(TOWNSDEV)/bin/i386-fmtowns

CC      = $(PREFIX)-gcc
LD      = $(PREFIX)-gcc
OBJCOPY = $(PREFIX)-objcopy
STRIP   = $(PREFIX)-strip

TARGET  = 240PFMT
EXE     = $(TARGET).EXP

DEST    = release
BIN     = bin
BUILD   = build

INCLUDES = -I. -I$(TOWNSDEV)/include

CFLAGS   = -std=c99 -Wall -Wextra -Wno-unused-parameter \
           -fomit-frame-pointer -O2 \
           -D__TOWNSDEV__ -D__FMTOWNS__ \
           $(INCLUDES)

LDFLAGS  = -L$(TOWNSDEV)/lib
LIBS     = -ltowns

OBJS = $(BUILD)/main.o \
       $(BUILD)/video.o \
       $(BUILD)/input.o \
       $(BUILD)/menu.o \
       $(BUILD)/patterns.o \
       $(BUILD)/font.o

all: $(BIN)/$(EXE)

$(BIN)/$(EXE): $(OBJS) | $(BIN) $(DEST)
	$(LD) $(LDFLAGS) -o $(BIN)/$(TARGET) $(OBJS) $(LIBS)
	$(OBJCOPY) -O fmtowns-exp $(BIN)/$(TARGET) $(BIN)/$(EXE)
	cp -v $(BIN)/$(EXE) $(DEST)/

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD) $(BIN) $(DEST):
	@mkdir -p $@

clean:
	rm -f $(BUILD)/*.o $(BIN)/$(TARGET) $(BIN)/$(EXE) $(DEST)/$(EXE)

.PHONY: all clean
