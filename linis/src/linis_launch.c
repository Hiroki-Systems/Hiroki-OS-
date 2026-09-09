/*
 * linis_launch.c — linis-launch: uygulama başlatıcı (LINIS_DESIGN.md §7)
 *
 * - .desktop dosyalarından uygulama listesi (/usr/share/applications,
 *   ~/.local/share/applications)
 * - yazıyla filtreleme, ↑/↓ seçim, Enter çalıştır, Esc kapat
 * - görünüm: yuvarlak köşe + gölge + blur kompozitör tarafından çizilir;
 *   bu process yalnızca içeriği çizer
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "linis_config.h"
#include "linis_util.h"

typedef struct {
	char *name;
	char *exec;
	int   term;     /* Terminal=true */
	int   shown;
} App;

typedef struct {
	Display *dpy;
	int      scr;
	Window   root;
	int      sw, sh;
	Window   win;
	GC       gc;
	Font     font;
	int      fh;
	LConfig *cfg;

	App     *apps;
	int      napps;
	int      cap;

	char     query[256];
	int      sel;
	int      vis;     /* ilk görünen sonuç */
} Launch;

/* ------------------------------------------------------------------ */
/* .desktop okuma                                                      */
/* ------------------------------------------------------------------ */

static void app_add(Launch *l, const char *name, const char *exec, int term)
{
	if (!name || !*name || !exec || !*exec)
		return;
	/* NoDisplay / OnlyShowIn kontrolü yapan üst katmanda yapılır */
	for (int i = 0; i < l->napps; i++)
		if (strcmp(l->apps[i].exec, exec) == 0 && strcmp(l->apps[i].name, name) == 0)
			return;
	if (l->napps == l->cap) {
		l->cap = l->cap ? l->cap * 2 : 64;
		l->apps = xrealloc(l->apps, (size_t)l->cap * sizeof(App));
	}
	App *a = &l->apps[l->napps++];
	a->name = xstrdup(name);
	a->exec = xstrdup(exec);
	a->term = term;
}

static void scan_desktops_in(Launch *l, const char *dirpath)
{
	DIR *d = opendir(dirpath);
	if (!d)
		return;
	struct dirent *de;
	while ((de = readdir(d))) {
		const char *nm = de->d_name;
		if (nm[0] == '.' || strlen(nm) < 9)
			continue;
		if (strcmp(nm + strlen(nm) - 8, ".desktop") != 0)
			continue;
		char path[1024];
		snprintf(path, sizeof(path), "%s/%s", dirpath, nm);
		FILE *f = fopen(path, "r");
		if (!f)
			continue;
		char line[1024];
		char name[512] = "", exec[1024] = "", icon[512] = "";
		int term = 0, hide = 0, nono = 0, inent = 0;
		while (fgets(line, sizeof(line), f)) {
			char *s = str_trim(line);
			if (!*s)
				continue;
			if (*s == '[') {
				inent = strcmp(s, "[Desktop Entry]") == 0;
				continue;
			}
			if (!inent)
				continue;
			if (str_startswith(s, "Name="))
				snprintf(name, sizeof(name), "%s", s + 5);
			else if (str_startswith(s, "Exec="))
				snprintf(exec, sizeof(exec), "%s", s + 5);
			else if (str_startswith(s, "Icon="))
				snprintf(icon, sizeof(icon), "%s", s + 5);
			else if (strcmp(s, "Terminal=true") == 0)
				term = 1;
			else if (strcmp(s, "NoDisplay=true") == 0)
				hide = 1;
			else if (str_startswith(s, "Hidden=true"))
				hide = 1;
			else if (str_startswith(s, "OnlyShowIn=") &&
			         strstr(s, "XFCE;") == NULL && strstr(s, "Linis;") == NULL)
				nono = 1;
		}
		fclose(f);
		if (hide || nono)
			continue;
		/* %f %F %u %U gibi alan kodlarını temizle */
		char clean[1024] = "";
		{
			int o = 0;
			for (int i = 0; exec[i] && i < (int)sizeof(exec) - 1; i++) {
				if (exec[i] == '%' && exec[i + 1]) {
					char c = exec[i + 1];
					if (c == 'f' || c == 'F' || c == 'u' || c == 'U' ||
					    c == 'i' || c == 'c' || c == 'k')
						i++;
					else
						clean[o++] = exec[i];
				} else
					clean[o++] = exec[i];
			}
			clean[o] = 0;
		}
		if (clean[0])
			app_add(l, name, clean, term);
	}
	closedir(d);
}

static void load_apps(Launch *l)
{
	const char *home = getenv("HOME");
	char p1[1024], p2[1024];
	snprintf(p1, sizeof(p1), "%s/.local/share/applications",
	         home ? home : "/root");
	snprintf(p2, sizeof(p2), "%s", "/usr/share/applications");
	scan_desktops_in(l, p1);
	scan_desktops_in(l, p2);
	/* yedek: dizinler yoksa birkaç standart */
	if (l->napps == 0) {
		app_add(l, "Terminal", l->cfg->term_cmd, 0);
		app_add(l, "Dosya Yoneticisi", l->cfg->file_cmd, 0);
		app_add(l, "Web Tarayici", "firefox", 0);
	}
	/* isme göre sırala (basit seçim sıralaması) */
	for (int i = 0; i < l->napps; i++)
		for (int j = i + 1; j < l->napps; j++)
			if (strcmp(l->apps[j].name, l->apps[i].name) < 0) {
				App t = l->apps[i];
				l->apps[i] = l->apps[j];
				l->apps[j] = t;
			}
}

/* ------------------------------------------------------------------ */
/* Filtreleme / çizim                                                  */
/* ------------------------------------------------------------------ */

static int matches(Launch *l, const App *a)
{
	if (!l->query[0])
		return 1;
	char q[256];
	snprintf(q, sizeof(q), "%s", l->query);
	str_to_lower(q);
	char n[1024];
	snprintf(n, sizeof(n), "%s %s", a->name, a->exec);
	str_to_lower(n);
	return strstr(n, q) != NULL;
}

static unsigned long xcol(Launch *l, uint32_t rgb)
{
	(void)l;
	return ((rgb >> 16) & 0xff) << 16 | ((rgb >> 8) & 0xff) << 8 | (rgb & 0xff);
}

static void fill(Launch *l, int x, int y, int w, int h, uint32_t c)
{
	XSetForeground(l->dpy, l->gc, xcol(l, c));
	XFillRectangle(l->dpy, l->win, l->gc, x, y, w, h);
}

static void draw_text(Launch *l, int x, int y, const char *s, uint32_t c)
{
	XSetForeground(l->dpy, l->gc, xcol(l, c));
	XSetFont(l->dpy, l->gc, l->font);
	XDrawString(l->dpy, l->win, l->gc, x, y, s, (int)strlen(s));
}

static void redraw(Launch *l)
{
	LConfig *cfg = l->cfg;
	int W, H;
	XWindowAttributes a;
	XGetWindowAttributes(l->dpy, l->win, &a);
	W = a.width;
	H = a.height;

	fill(l, 0, 0, W, H, cfg->colors.bg_mid);

	/* arama çubuğu */
	char qline[300];
	snprintf(qline, sizeof(qline), "  Ara: %s", l->query);
	draw_text(l, 10, 30, qline, cfg->colors.fg_primary);
	fill(l, 8, 42, W - 16, 2, cfg->colors.accent_blue);

	/* görünür sonuç sayısı */
	int total = 0;
	for (int i = 0; i < l->napps; i++)
		if (matches(l, &l->apps[i]))
			total++;
	if (total == 0) {
		draw_text(l, 12, 76, "  Sonuc yok", cfg->colors.fg_dim);
		return;
	}
	/* seçimi görünür aralığa taşı */
	if (l->sel < 0)
		l->sel = 0;
	if (l->sel >= total)
		l->sel = total - 1;
	int maxrows = (H - 56) / 38;
	if (l->vis > l->sel)
		l->vis = l->sel;
	if (l->vis + maxrows <= l->sel)
		l->vis = l->sel - maxrows + 1;
	if (l->vis < 0)
		l->vis = 0;

	int ry = 52;
	int shown = 0;
	for (int i = 0; i < l->napps && shown < maxrows; i++) {
		App *ap = &l->apps[i];
		if (!matches(l, ap))
			continue;
		shown++;
		if (shown - 1 < l->vis)
			continue;
		int selected = (shown - 1 == l->sel);
		if (selected)
			fill(l, 8, ry, W - 16, 38, cfg->colors.bg_light);
		/* ikon (harf) */
		XSetForeground(l->dpy, l->gc, xcol(l, cfg->colors.accent_cyan));
		XDrawArc(l->dpy, l->win, l->gc, 12, ry + 3, 32, 32, 0, 360 * 64);
		char ch = ap->name[0] ? ap->name[0] : '?';
		char buf[2] = { ch, 0 };
		draw_text(l, 22, ry + 25, buf, selected ? cfg->colors.bg_mid
		                                         : cfg->colors.accent_cyan);
		draw_text(l, 52, ry + 20, ap->name, selected
		                                     ? cfg->colors.fg_primary
		                                     : cfg->colors.fg_primary);
		draw_text(l, 52, ry + 32, ap->exec, cfg->colors.fg_dim);
		ry += 40;
	}
	/* sol seçim çubuğu */
	if (total) {
		int sy = 52 + l->sel * 40;
		(void)sy;
	}
}

/* ------------------------------------------------------------------ */
/* Başlatma / kapatma                                                  */
/* ------------------------------------------------------------------ */

static void launch_app(Launch *l, App *a)
{
	char cmd[2048];
	if (a->term) {
		snprintf(cmd, sizeof(cmd), "%s -e %s", l->cfg->term_cmd, a->exec);
	} else {
		snprintf(cmd, sizeof(cmd), "%s", a->exec);
	}
	spawn(cmd);
}

static void launcher_close(Launch *l)
{
	XUnmapWindow(l->dpy, l->win);
	XFlush(l->dpy);
}

/* ------------------------------------------------------------------ */
/* Olaylar                                                             */
/* ------------------------------------------------------------------ */

static int visible_total(Launch *l)
{
	int n = 0;
	for (int i = 0; i < l->napps; i++)
		if (matches(l, &l->apps[i]))
			n++;
	return n;
}

static App *visible_at(Launch *l, int k)
{
	int c = 0;
	for (int i = 0; i < l->napps; i++) {
		if (!matches(l, &l->apps[i]))
			continue;
		if (c == k)
			return &l->apps[i];
		c++;
	}
	return NULL;
}

static void on_key(Launch *l, XKeyEvent *ev)
{
	KeySym ks = XLookupKeysym(ev, 0);
	if (ks == XK_Escape) {
		launcher_close(l);
		return;
	}
	if (ks == XK_Return || ks == XK_KP_Enter) {
		int n = visible_total(l);
		if (n > 0 && l->sel >= 0 && l->sel < n) {
			App *a = visible_at(l, l->sel);
			if (a)
				launch_app(l, a);
		}
		launcher_close(l);
		return;
	}
	if (ks == XK_Up || ks == XK_KP_Up) {
		if (l->sel > 0)
			l->sel--;
		redraw(l);
		return;
	}
	if (ks == XK_Down || ks == XK_KP_Down) {
		int n = visible_total(l);
		if (l->sel < n - 1)
			l->sel++;
		redraw(l);
		return;
	}
	if (ks == XK_BackSpace) {
		int len = (int)strlen(l->query);
		if (len > 0) {
			l->query[len - 1] = 0;
			l->sel = 0;
			l->vis = 0;
		}
		redraw(l);
		return;
	}
	if (ks >= 0x20 && ks < 0x100) {
		int len = (int)strlen(l->query);
		if (len < (int)sizeof(l->query) - 1) {
			l->query[len] = (char)ks;
			l->query[len + 1] = 0;
			l->sel = 0;
			l->vis = 0;
		}
		redraw(l);
		return;
	}
}

static void on_mouse(Launch *l, XButtonEvent *ev)
{
	int n = visible_total(l);
	if (ev->button != Button1)
		return;
	if (ev->y < 52)
		return;
	int row = (ev->y - 52) / 40;
	if (row >= 0 && row < n) {
		App *a = visible_at(l, row);
		if (a)
			launch_app(l, a);
		launcher_close(l);
	}
}

/* ------------------------------------------------------------------ */
/* Ana                                                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
	Display *dpy = XOpenDisplay(NULL);
	if (!dpy)
		return 1;

	LConfig cfg;
	config_defaults(&cfg);
	{
		const char *paths[4];
		int n = 0;
		char *p;
		if ((p = linis_find_file("config.toml")))
			paths[n++] = p;
		if ((p = linis_find_file("theme.toml")))
			paths[n++] = p;
		else if ((p = linis_find_file("themes/tokyo-night.toml")))
			paths[n++] = p;
		config_load(&cfg, paths, n);
		for (int i = 0; i < n; i++)
			free((void *)paths[i]);
	}

	Launch *l = xcalloc(1, sizeof(*l));
	l->dpy = dpy;
	l->scr = DefaultScreen(dpy);
	l->root = RootWindow(dpy, l->scr);
	l->cfg = &cfg;
	{
		Screen *s = ScreenOfDisplay(dpy, l->scr);
		l->sw = WidthOfScreen(s);
		l->sh = HeightOfScreen(s);
	}
	l->gc = XCreateGC(dpy, l->root, 0, NULL);
	l->font = XLoadFont(dpy, "fixed");
	l->fh = 14;
	load_apps(l);

	int W = LMIN(760, l->sw * 3 / 5);
	int H = LMIN(560, l->sh * 3 / 4);
	int x = (l->sw - W) / 2;
	int y = (l->sh - H) / 2 - 30;
	if (y < 0)
		y = 0;

	XSetWindowAttributes wa;
	wa.override_redirect = True;
	wa.background_pixel = xcol(l, cfg.colors.bg_mid);
	wa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask;
	l->win = XCreateWindow(dpy, l->root, x, y, W, H, 1,
	                       CopyFromParent, InputOutput, CopyFromParent,
	                       CWOverrideRedirect | CWBackPixel | CWEventMask,
	                       &wa);
	/* kompozitör efekti: %92 opaklık + blur + tip normal */
	unsigned long op = (unsigned long)(0.92 * 0xffff);
	XChangeProperty(dpy, l->win, XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False),
	                XA_CARDINAL, 32, PropModeReplace,
	                (const unsigned char *)&op, 1);
	unsigned long blur = 20;
	XChangeProperty(dpy, l->win, XInternAtom(dpy, "_LINIS_BLUR_RADIUS", False),
	                XA_CARDINAL, 32, PropModeReplace,
	                (const unsigned char *)&blur, 1);
	Atom atype = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	Atom dlg = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	XChangeProperty(dpy, l->win, atype, XA_ATOM, 32, PropModeReplace,
	                (const unsigned char *)&dlg, 1);

	/* üstte kal & odağı al */
	XMapWindow(dpy, l->win);
	XRaiseWindow(dpy, l->win);
	XSetInputFocus(dpy, l->win, RevertToParent, CurrentTime);
	redraw(l);
	XFlush(dpy);

	int fd = ConnectionNumber(dpy);
	int done = 0;
	while (!done) {
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 100 * 1000;
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		select(fd + 1, &rfds, NULL, NULL, &tv);
		while (XPending(dpy) && !done) {
			XEvent e;
			XNextEvent(dpy, &e);
			switch (e.type) {
			case Expose:
				redraw(l);
				break;
			case KeyPress:
				on_key(l, &e.xkey);
				break;
			case ButtonPress:
				on_mouse(l, &e.xbutton);
				break;
			case FocusOut:
				launcher_close(l);
				break;
			case DestroyNotify:
				done = 1;
				break;
			default:
				break;
			}
		}
		/* kapatıldıysa çık */
		XWindowAttributes a;
		if (XGetWindowAttributes(dpy, l->win, &a) &&
		    a.map_state == IsUnmapped)
			done = 1;
	}
	XDestroyWindow(dpy, l->win);
	XCloseDisplay(dpy);
	return 0;
}
