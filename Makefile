CC = gcc

SRC_DIR = src
DIST_DIR = dist
BIN_DIR = bin
LIBS_DIR = libs
PROTOCOLS_DIR = protocols

CPPFLAGS = -I$(LIBS_DIR) -I$(LIBS_DIR)/libxdwayland/src
LDLIBS = -lm

PY_SRCS = $(wildcard $(LIBS_DIR)/*/src/*.py)
PY_INSTALL_DIR = $(HOME)/.local/bin

PROTOCOL_SRCS = \
    wlr-layer-shell-unstable-v1-protocol.c \
    xdg-output-unstable-v1-protocol.c \
    viewporter-protocol.c


MAIN_SRCS = $(SRC_DIR)/xdwlw-ipc.c $(SRC_DIR)/xdwlw.c $(SRC_DIR)/xdwlw-error.c
MAIN_OBJS = $(MAIN_SRCS:%.c=$(DIST_DIR)/%.o)

DAEMON_SRCS = $(PROTOCOL_SRCS) $(SRC_DIR)/xdwlw-handlers.c $(SRC_DIR)/xdwlw-ipc.c $(SRC_DIR)/xdwlw-outputs.c $(SRC_DIR)/xdwlwd.c $(SRC_DIR)/xdwlw-error.c $(wildcard $(LIBS_DIR)/*/src/*.c)
DAEMON_OBJS = $(DAEMON_SRCS:%.c=$(DIST_DIR)/%.o)

DEBUG = 
# DEBUG = -DDEBUG

MAIN = $(BIN_DIR)/xdwlw
DAEMON = $(BIN_DIR)/xdwlwd

MAIN_DEBUG = $(BIN_DIR)/debug
DAEMON_DEBUG = $(BIN_DIR)/debugd

.PHONY: all clean debug main xdwayland-scanner

all: main

main: $(MAIN) $(DAEMON) xdwayland-scanner
main: CFLAGS = -Wall -Wextra -Wno-unused-function -Wno-unused-parameter

debug: $(MAIN_DEBUG) $(DAEMON_DEBUG) xdwayland-scanner
debug: CFLAGS = -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -g


$(MAIN): $(MAIN_OBJS)
	mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LDLIBS)

$(DAEMON): $(DAEMON_OBJS)
	mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LDLIBS)

$(MAIN_DEBUG): $(MAIN_SRCS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -DLOGS $(DEBUG) -o $@ $^ $(LDLIBS)

$(DAEMON_DEBUG): $(DAEMON_SRCS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -DLOGS $(DEBUG) -o $@ $^ $(LDLIBS)

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

install:
	cp $(MAIN) /usr/bin/
	cp $(DAEMON) /usr/bin/

uninstall:
	rm -f /usr/bin/$(basename $(MAIN))
	rm -f /usr/bin/$(basename $(DAEMON))
	rm -f $(PY_INSTALL_DIR)/xdwayland-scanner

clean:
	rm -rf $(DIST_DIR) $(BIN_DIR)
	rm -f *protocol.*

