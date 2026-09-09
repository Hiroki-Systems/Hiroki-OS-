# Linis Window Manager — Makefile
#
# WM bağımlılıkları: libx11 libxrender libxcomposite libxdamage libxfixes
#                     libxext libxrandr libxinerama libxft fontconfig freetype2
# Panel bağımlılıkları: gtk+-3.0 cairo

CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L
LDFLAGS := $(LDFLAGS)
PREFIX  ?= /usr
DESTDIR ?=
BINDIR   = $(DESTDIR)$(PREFIX)/bin
SHRDIR   = $(DESTDIR)$(PREFIX)/share/linis
XSDIR    = $(DESTDIR)$(PREFIX)/share/xsessions
XDGIR    = $(DESTDIR)/etc/xdg/linis

SRC = src
COMMON = $(SRC)/linis_util.o $(SRC)/linis_config.o $(SRC)/linis_anim.o
XLIBS  = -lX11 -lXrender -lXcomposite -lXdamage -lXfixes -lXext \
          -lXrandr -lXinerama -lXft -lfontconfig -lfreetype \
          -lpthread -lm

GTK_CFLAGS  = $(shell pkg-config --cflags gtk+-3.0 cairo 2>/dev/null || echo "-I/usr/include/gtk-3.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/cairo -I/usr/include/pango-1.0 -I/usr/include/harfbuzz -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/atk-1.0")
GTK_LDFLAGS = $(shell pkg-config --libs gtk+-3.0 cairo 2>/dev/null || echo "-lgtk-3 -lgdk-3 -lglib-2.0 -lcairo -lpango-1.0 -lpangocairo-1.0 -lgdk_pixbuf-2.0")

all: linis linis-panel linis-launch

linis: $(SRC)/linis.c $(SRC)/linis_wm.c $(SRC)/linis_compositor.c $(SRC)/linis_xsurf.c \
       $(SRC)/linis_keybind.c $(SRC)/linis_ewmh.c $(SRC)/linis_bootvid.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(XLIBS)

linis-panel: $(SRC)/linis_panel.c $(SRC)/linis_ewmh.c $(SRC)/linis_util.o $(SRC)/linis_config.o
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -o $@ $^ $(LDFLAGS) $(XLIBS) $(GTK_LDFLAGS)

linis-launch: $(SRC)/linis_launch.c $(SRC)/linis_util.o $(SRC)/linis_config.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(XLIBS)

$(SRC)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

install: all
	install -d $(BINDIR) $(SHRDIR)/themes $(SHRDIR)/wallpapers \
	         $(SHRDIR)/videos $(XSDIR) $(XDGIR)
	install -m 0755 linis linis-panel linis-launch $(BINDIR)/
	install -m 0755 files/usr/bin/linis-session files/usr/bin/linis-restart \
	        $(BINDIR)/
	install -m 0644 files/usr/share/linis/themes/tokyo-night.toml \
	        $(SHRDIR)/themes/
	install -m 0644 files/usr/share/linis/keybinds.conf $(SHRDIR)/
	install -m 0755 files/usr/share/linis/autostart $(SHRDIR)/
	install -m 0644 files/usr/share/linis/logo.png $(SHRDIR)/
	install -m 0644 files/usr/share/linis/wallpapers/sakura-gradient.jpg \
	        $(SHRDIR)/wallpapers/
	install -m 0644 files/etc/xdg/linis/config.toml $(XDGIR)/
	install -m 0644 files/usr/share/xsessions/linis.desktop $(XSDIR)/

uninstall:
	rm -f $(BINDIR)/linis $(BINDIR)/linis-panel $(BINDIR)/linis-launch \
	      $(BINDIR)/linis-session $(BINDIR)/linis-restart
	rm -rf $(SHRDIR) $(XDGIR)
	rm -f $(XSDIR)/linis.desktop

clean:
	rm -f $(SRC)/*.o linis linis-panel linis-launch

.PHONY: all install uninstall clean
