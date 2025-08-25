CC = gcc

SRC_DIR = src
DIST_DIR = dist
BIN_DIR = bin
LIBS_DIR = libs
PROTOCOLS_DIR = protocols

CPPFLAGS = -I/usr/include/libxml2 -I$(LIBS_DIR)/libxdwayland/src
LDLIBS = -lxml2

PY_SRCS = $(wildcard $(LIBS_DIR)/*/src/*.py)
PY_INSTALL_DIR = $(HOME)/.local/bin

PROTOCOL_SRCS = \
    wlr-layer-shell-unstable-v1-protocol.c \
    xdg-output-unstable-v1-protocol.c \
    viewporter-protocol.c

SRCS = $(PROTOCOL_SRCS) $(wildcard $(SRC_DIR)/*.c) $(wildcard $(LIBS_DIR)/*/src/*.c)
OBJS = $(SRCS:%.c=$(DIST_DIR)/%.o)


# LOGS =
LOGS = -DLOGS

TARGET = $(BIN_DIR)/main
DEBUG = $(BIN_DIR)/debug

.PHONY: all clean debug main xdwayland-scanner

all: main

main: $(TARGET) xdwayland-scanner
main: CFLAGS = -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -O2

debug: $(DEBUG) xdwayland-scanner
debug: CFLAGS = -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -g


$(TARGET): $(OBJS)
	mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LDLIBS)

$(DEBUG): $(SRCS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -DLOGS -o $@ $^ $(LDLIBS)

$(DIST_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(LOGS) -o $@ $<

xdwayland-scanner: $(LIBS_DIR)/libxdwayland/src/xdwayland-scanner.py
	mkdir -p $(PY_INSTALL_DIR)
	install -m 755 $< $(PY_INSTALL_DIR)/$@

wlr-layer-shell-unstable-v1-protocol.c: xdwayland-scanner
	xdwayland-scanner $(PROTOCOLS_DIR)/wlr-layer-shell-unstable-v1.xml wlr-layer-shell-unstable-v1-protocol

xdg-output-unstable-v1-protocol.c: xdwayland-scanner
	xdwayland-scanner /usr/share/wayland-protocols/unstable/xdg-output/xdg-output-unstable-v1.xml xdg-output-unstable-v1-protocol

viewporter-protocol.c: xdwayland-scanner
	xdwayland-scanner /usr/share/wayland-protocols/stable/viewporter/viewporter.xml viewporter-protocol


clean:
	rm -rf $(DIST_DIR) $(BIN_DIR)
	rm -f *protocol.*

