/*
 * linis.c — Linis WM ana process (giriş + olay döngüsü).
 *
 * Oturum akışı (LINIS_DESIGN.md §11):
 *   xinit → linis-session → wallpaper + linis → linis-panel & arkadaşlar
 *
 * linis, WM + built-in compositor'ü aynı process'te çalıştırır.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>

#include <X11/Xlib.h>

#include "linis_config.h"
#include "linis_wm.h"
#include "linis_util.h"

#define LINIS_VERSION "1.0.0"

static void usage(void)
{
	printf("linis v%s — Tokyo Night tarzı X11 window manager\n", LINIS_VERSION);
	printf("Kullanim: linis [-h] [-v] [-d seviye]\n");
	printf("  -d N     log seviyesi (0=hata 1=uyari 2=bilgi 3=dbg)\n");
}

int main(int argc, char **argv)
{
	int loglvl = 2;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			usage();
			return 0;
		}
		if (strcmp(argv[i], "-v") == 0 ||
		    strcmp(argv[i], "--version") == 0) {
			printf("linis v%s\n", LINIS_VERSION);
			return 0;
		}
		if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
			loglvl = atoi(argv[++i]);
		}
	}
	llog_set_level(loglvl);

	Display *dpy = XOpenDisplay(NULL);
	if (!dpy)
		die("X sunucusuna baglanilamadi (DISPLAY=%s)",
		    getenv("DISPLAY") ? getenv("DISPLAY") : "bos");

	/* config + tema */
	LConfig cfg;
	config_defaults(&cfg);
	{
		const char *paths[8];
		int n = 0;
		char *p;
		/* öncelik: kullanıcı > sistem > paket teması */
		char *cfgd = linis_config_dir();
		(void)cfgd;
		if ((p = linis_find_file("config.toml")))
			paths[n++] = p;
		/* tema dosyaları aynı parser ile renk/compositor/panel değerlerini
		 * config üzerine bindirir. */
		if ((p = linis_find_file("theme.toml")))
			paths[n++] = p;
		else if ((p = linis_find_file("themes/tokyo-night.toml")))
			paths[n++] = p;
		paths[n] = NULL;
		config_load(&cfg, paths, n);
		for (int i = 0; i < n; i++)
			free((void *)paths[i]);
	}

	LinisWM *wm = wm_create(dpy, &cfg);
	llog(2, "linis hazir (%d workspace, tema yuklu)",
	     cfg.workspace_count);

	int fd = ConnectionNumber(dpy);
	for (;;) {
		if (wm_wants_restart(wm)) {
			llog(2, "yeniden baslatiliyor...");
			wm_destroy(wm);
			XCloseDisplay(dpy);
			/* eski bağlantı kapanmadan yenisi açılmasın diye kısa gecikme */
			spawn("sleep 0.2 && exec linis");
			return 0;
		}
		if (wm_wants_quit(wm))
			break;

		int t = wm_timeout_ms(wm);
		struct timeval tv;
		tv.tv_sec = t / 1000;
		tv.tv_usec = (t % 1000) * 1000;

		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		int r = select(fd + 1, &rfds, NULL, NULL, &tv);
		if (r < 0 && errno != EINTR)
			break;

		while (XPending(dpy) && !wm_wants_quit(wm) && !wm_wants_restart(wm)) {
			XEvent ev;
			XNextEvent(dpy, &ev);
			wm_handle_event(wm, &ev);
		}
		wm_loop_tick(wm);
	}

	llog(2, "kapaniyor");
	wm_destroy(wm);
	XCloseDisplay(dpy);
	return 0;
}
