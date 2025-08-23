CC = gcc

SRC_DIR = src
DIST_DIR = dist
BIN_DIR = bin
LIBS_DIR = libs
PROTOCOLS_DIR = protocols

CFLAGS = -Wno-unused-function -Wno-unused-parameter -fPIC -O2
CPPFLAGS = -I/usr/include/libxml2 -I$(LIBS_DIR)/libxdwayland/src
LDLIBS = -lxml2

PY_SRCS = $(wildcard $(LIBS_DIR)/*/src/*.py)
PY_INSTALL_DIR = $(HOME)/.local/bin

SRCS = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(LIBS_DIR)/*/src/*.c)
OBJS = $(SRCS:%.c=$(DIST_DIR)/%.o)

PROTOCOLS = wlr-layer-shell-unstable-v1-protocol.c viewporter-protocol.c

# LOGS =
LOGS = -DLOGS

TARGET = $(BIN_DIR)/main

all: $(TARGET) xdwl-scanner

$(TARGET): clean $(OBJS) $(PROTOCOLS)
	mkdir -p $(dir $@)
	$(CC) -o $@ $(CPPFLAGS) $(OBJS) $(PROTOCOLS) $(LDLIBS)

$(DIST_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(LOGS) $(CFLAGS) -c $< -o $@

xdwl-scanner: $(LIBS_DIR)/libxdwayland/src/scanner.py
	mkdir -p $(PY_INSTALL_DIR)
	install -m 755 $< $(PY_INSTALL_DIR)/$@

wlr-layer-shell-unstable-v1-protocol.c: xdwl-scanner
	xdwl-scanner $(PROTOCOLS_DIR)/wlr-layer-shell-unstable-v1.xml $(basename $@)

viewporter-protocol.c: xdwl-scanner
	xdwl-scanner $(PROTOCOLS_DIR)/viewporter.xml $(basename $@)

clean:
	rm -rf $(DIST_DIR) $(BIN_DIR)
	rm -f *protocol.*

