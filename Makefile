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

PROTOCOLS = wlr-layer-shell-unstable-v1-protocol.c

# LOGS =
LOGS = -DLOGS

TARGET = $(BIN_DIR)/main

all: $(TARGET) xdwayland-scanner

$(TARGET): clean $(PROTOCOLS) $(OBJS)
	mkdir -p $(dir $@)
	$(CC) -o $@ $(CPPFLAGS) $(OBJS) $(PROTOCOLS) $(LDLIBS)

$(DIST_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(LOGS) $(CFLAGS) -c $< -o $@

xdwayland-scanner: $(LIBS_DIR)/libxdwayland/src/xdwayland-scanner.py
	mkdir -p $(PY_INSTALL_DIR)
	install -m 755 $< $(PY_INSTALL_DIR)/$@

wlr-layer-shell-unstable-v1-protocol.c: xdwayland-scanner
	xdwayland-scanner $(PROTOCOLS_DIR)/wlr-layer-shell-unstable-v1.xml $(basename $@)

clean:
	rm -rf $(DIST_DIR) $(BIN_DIR)
	rm -f *protocol.*

