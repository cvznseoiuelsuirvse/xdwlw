CC = gcc

SRC_DIR = src
DIST_DIR = dist
BIN_DIR = bin
LIBS_DIR = libs
PROTOCOLS_DIR = protocols

SYS_BIN_DIR = /usr/bin

CPPFLAGS = -I$(LIBS_DIR)/ -I$(LIBS_DIR)/libxdwayland/src
LDLIBS = -lm

PROTOCOL_SRCS = \
    wlr-layer-shell-unstable-v1-protocol.c \
    xdg-output-unstable-v1-protocol.c \
    viewporter-protocol.c


MAIN_SRCS = $(SRC_DIR)/xdwlw-ipc.c $(SRC_DIR)/xdwlw.c $(SRC_DIR)/xdwlw-error.c
MAIN_OBJS = $(MAIN_SRCS:%.c=$(DIST_DIR)/%.o)

DAEMON_SRCS = $(PROTOCOL_SRCS) $(SRC_DIR)/xdwlw-ipc.c $(SRC_DIR)/xdwlwd.c $(SRC_DIR)/xdwlw-error.c $(wildcard $(LIBS_DIR)/*/src/*.c) $(wildcard *protocol.c)
DAEMON_OBJS = $(DAEMON_SRCS:%.c=$(DIST_DIR)/%.o)

MAIN = $(BIN_DIR)/xdwlw
DAEMON = $(BIN_DIR)/xdwlwd
SCANNER = $(LIBS_DIR)/libxdwayland/src/xdwayland-scanner.py

MAIN_DEBUG = $(BIN_DIR)/debug
DAEMON_DEBUG = $(BIN_DIR)/debugd

.PHONY: all clean debug main install uninstall

all: main

main: $(MAIN) $(DAEMON)
main: CFLAGS = -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -O3

debug: $(MAIN_DEBUG) $(DAEMON_DEBUG)
debug: CFLAGS = -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -g


$(MAIN): $(MAIN_OBJS)
	mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LDLIBS)

$(DAEMON): $(DAEMON_OBJS)
	mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LDLIBS)

$(MAIN_DEBUG): $(MAIN_SRCS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -DLOGS -o $@ $^ $(LDLIBS)

$(DAEMON_DEBUG): $(DAEMON_SRCS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -DLOGS -o $@ $^ $(LDLIBS)

$(DIST_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(LOGS) -o $@ $<

wlr-layer-shell-unstable-v1-protocol.c:
	$(SCANNER) $(PROTOCOLS_DIR)/wlr-layer-shell-unstable-v1.xml wlr-layer-shell-unstable-v1-protocol

xdg-output-unstable-v1-protocol.c:
	$(SCANNER) /usr/share/wayland-protocols/unstable/xdg-output/xdg-output-unstable-v1.xml xdg-output-unstable-v1-protocol

viewporter-protocol.c:
	$(SCANNER) /usr/share/wayland-protocols/stable/viewporter/viewporter.xml viewporter-protocol

install:
	cp $(MAIN) $(SYS_BIN_DIR)
	cp $(DAEMON) $(SYS_BIN_DIR)

uninstall:
	rm -f /usr/bin/$(notdir $(MAIN))
	rm -f /usr/bin/$(notdir $(DAEMON))

clean:
	rm -rf $(DIST_DIR) $(BIN_DIR)
	rm -f *protocol.*

