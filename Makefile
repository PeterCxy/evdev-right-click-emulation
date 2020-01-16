CC := gcc
XFLAGS := -Wall -std=c11 -D_POSIX_C_SOURCE=199309L
LIBRARIES := -levdev
INCLUDES := -I/usr/include/libevdev-1.0
CFLAGS := $(XFLAGS) $(INCLUDES)

OUTDIR := out
SOURCES := uinput.c input.c rce.c
OBJS := $(SOURCES:%.c=$(OUTDIR)/%.o)
TARGET := $(OUTDIR)/evdev-rce

.PHONY: all clean

$(OUTDIR)/%.o: %.c
	@mkdir -p $(OUTDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBRARIES) -o $@

all: $(TARGET)
clean:
	rm -rf $(OUTDIR)