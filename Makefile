TOOLCHAIN_PREFIX ?= /opt/arm-mingw32ce/bin/arm-mingw32ce-
CC = $(TOOLCHAIN_PREFIX)gcc
STRIP = $(TOOLCHAIN_PREFIX)strip
WINDRES = $(TOOLCHAIN_PREFIX)windres

CFLAGS = -O2 -Wall -Wextra -std=c99 -pedantic -D_WIN32_WCE=0x600 -D_UNICODE -DUNICODE
LDFLAGS = -laygshell -lcommctrl -lcommdlg -lcoredll -static-libgcc

SRC = src/main.c src/core.c src/canvas.c src/tools.c src/ui.c src/fileio.c src/text.c
OBJ = $(SRC:.c=.o)
RES = src/resources.o
TARGET = cepbrush.exe

all: $(TARGET)

$(TARGET): $(OBJ) $(RES)
	$(CC) $(OBJ) $(RES) -o $@ $(LDFLAGS)
	$(STRIP) $@

src/%.o: src/%.c src/app.h src/resource.h
	$(CC) $(CFLAGS) -Isrc -c $< -o $@

src/resources.o: src/resources.rc src/resource.h
	$(WINDRES) -Isrc $< $@

clean:
	rm -f $(OBJ) $(RES) $(TARGET)
