CC = gcc
CFLAGS = -Wall -Wextra -g $(shell pkg-config --cflags glib-2.0)
LDFLAGS = $(shell pkg-config --libs glib-2.0)

# Detect optional dependencies
PKG_CONFIG ?= pkg-config

# X11
HAVE_X11 := $(shell $(PKG_CONFIG) --exists x11 && echo 1)
ifeq ($(HAVE_X11),1)
    CFLAGS += -DCHEETER_HAVE_X11 $(shell $(PKG_CONFIG) --cflags x11)
    LDFLAGS += $(shell $(PKG_CONFIG) --libs x11)
endif

# AT-SPI
HAVE_ATSPI := $(shell $(PKG_CONFIG) --exists atspi-2 && echo 1)
ifeq ($(HAVE_ATSPI),1)
    CFLAGS += -DCHEETER_HAVE_ATSPI $(shell $(PKG_CONFIG) --cflags atspi-2)
    LDFLAGS += $(shell $(PKG_CONFIG) --libs atspi-2)
endif

# GTK+3 and Poppler
# Assuming these are core for the viewer, even if UI is minimal
CFLAGS += $(shell $(PKG_CONFIG) --cflags gtk+-3.0 poppler-glib)
LDFLAGS += $(shell $(PKG_CONFIG) --libs gtk+-3.0 poppler-glib)

SRC_CORE = src/core/log.c src/core/config.c src/core/paths.c
SRC_PHASE1 = src/index/index_scan.c src/mapping/mappings_store.c src/mapping/resolve.c
SRC_BACKEND = src/backend/backend_common.c src/backend/x11/x11_backend.c src/backend/wayland/wayland_backend.c
SRC_UI = src/ui/ui_manager.c src/ui/viewer_poppler.c
SRC_IPC = src/ipc/ipc_server.c src/ipc/ipc_client.c
SRC_DAEMON = src/cheeterd.c $(SRC_CORE) $(SRC_PHASE1) $(SRC_BACKEND) $(SRC_UI) $(SRC_IPC)
SRC_CLI = src/cheeter.c $(SRC_CORE) src/ipc/ipc_client.c

OBJ_DAEMON = $(SRC_DAEMON:.c=.o)
OBJ_CLI = $(SRC_CLI:.c=.o)

all: cheeter cheeterd

cheeter: $(OBJ_CLI)
	$(CC) -o $@ $^ $(LDFLAGS)

cheeterd: $(OBJ_DAEMON)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -Iinclude -c -o $@ $<

# Install variables
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share
LIBDIR ?= $(PREFIX)/lib

install: all
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 cheeter $(DESTDIR)$(BINDIR)/cheeter
	install -m 755 cheeterd $(DESTDIR)$(BINDIR)/cheeterd
	
	install -d $(DESTDIR)$(DATADIR)/applications
	install -m 644 assets/cheeter.desktop $(DESTDIR)$(DATADIR)/applications/cheeter.desktop

	install -d $(DESTDIR)$(LIBDIR)/systemd/user
	install -m 644 assets/cheeter.service $(DESTDIR)$(LIBDIR)/systemd/user/cheeter.service

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/cheeter
	rm -f $(DESTDIR)$(BINDIR)/cheeterd
	rm -f $(DESTDIR)$(DATADIR)/applications/cheeter.desktop
	rm -f $(DESTDIR)$(LIBDIR)/systemd/user/cheeter.service

clean:
	rm -f $(OBJ_DAEMON) $(OBJ_CLI) cheeter cheeterd

run: cheeterd
	./cheeterd

.PHONY: all clean install uninstall run
