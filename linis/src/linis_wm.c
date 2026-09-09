/*
 * linis_wm.c — Linis Window Manager çekirdeği.
 *
 * Sorumluluklar:
 *  - EWMH/ICCCM kök özellikleri + pencere yönetimi (DESIGN §3)
 *  - floating / tiling / fullscreen / monocle
 *  - workspace yönetimi
 *  - built-in compositor'ü sürme
 *  - keybinds.conf dağıtımı + XGrabKey
 *  - Mod4+LMB sürükle / Mod4+RMB boyutlandır
 *  - strut tabanlı workarea hesabı
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <math.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "linis_wm.h"
#include "linis_compositor.h"
#include "linis_keybind.h"
#include "linis_ewmh.h"
#include "linis_bootvid.h"
#include "linis_anim.h"
#include "linis_config.h"
#include "linis_util.h"

static LinisWM *g_wm = NULL;

#define MAXWS       9
#define MOD_MOVE    Mod4Mask
#define DIM(x)  (x)

typedef struct LinisClient LinisClient;

struct LinisClient {
	Window   w;
	LinisClient *next;
	int      idx;             /* sıra */
	int      ws;              /* workspace */
	int      f;               /* floating override */
	int      isfull;          /* fullscreen */
	int      ic;              /* iconified */
	int      isdock;
	int      isdialog;
	int      isdesktop;
	int      evermapped;
	int      x, y, wd, ht;
	int      fx, fy, fw, fh;  /* floating geometri */
	int      ox, oy, ow, oh;  /* fullscreen öncesi */
	char    *name;
};

struct LinisWM {
	Display *dpy;
	int      scr;
	Window   root;
	int      sw, sh;
	LConfig *cfg;

	Atom     net[NET_LAST];
	Atom     ic_extra[3];     /* WM_PROTOCOLS WM_DELETE_WINDOW WM_TAKE_FOCUS */
	Window   check;

	LCompositor *comp;
	LKeybinds *kb;
	int      have_keyconf;

	LinisClient *clients;
	LinisClient *focus;

	int      curws;
	int      nws;
	char     wsmode[MAXWS];   /* 0 floating, 1 tiling */
	char     wsmono[MAXWS];

	int      do_quit;
	int      do_restart;

	/* fare sürükleme durumu */
	LinisClient *drag;
	int      dragmode;        /* 0 move 1 resize */
	int      dragx, dragy;
	int      dwin_x, dwin_y, dwin_w, dwin_h;

	/* workarea (strut düşülmüş) */
	int      wa_x, wa_y, wa_w, wa_h;
	int      strut_l, strut_r, strut_t, strut_b;
};

/* ------------------------------------------------------------------ */
/* Temel yardımcılar                                                   */
/* ------------------------------------------------------------------ */

static LinisClient *find_client(LinisWM *wm, Window w)
{
	for (LinisClient *c = wm->clients; c; c = c->next)
		if (c->w == w)
			return c;
	return NULL;
}

static int rect_inset_valid(LinisWM *wm)
{
	return wm->wa_w > 50 && wm->wa_h > 50;
}

static void set_hidden_state(LinisWM *wm, Window w, int hidden)
{
	ewmh_set_state_flags(wm->dpy, w, wm->net[NET_WM_STATE],
	                     wm->net[NET_WM_STATE_FULLSCREEN],
	                     wm->net[NET_WM_STATE_HIDDEN], -1, hidden);
}

/* _NET_WM_DESKTOP döndür (yoksa -1) */
static int get_wm_desktop(LinisWM *wm, Window w)
{
	long v = -1;
	if (ewmh_get_cardinal(wm->dpy, w, wm->net[NET_WM_DESKTOP], &v))
		return (int)v;
	return -1;
}

static const char *win_class(LinisWM *wm, Window w)
{
	static char buf[128];
	char *cls = ewmh_get_class(wm->dpy, w);
	if (!cls)
		return "";
	snprintf(buf, sizeof(buf), "%s", cls);
	free(cls);
	return buf;
}

/* ------------------------------------------------------------------ */
/* Hata yönetimi                                                       */
/* ------------------------------------------------------------------ */

static int xerr_ignore(Display *d, XErrorEvent *e)
{
	/* BadAccess genellikle başka bir WM'den gelir; ama MapRequest vs.
	 * esnasında da oluşabilir. Sessizce yut. */
	(void)d; (void)e;
	return 0;
}

/* ------------------------------------------------------------------ */
/* Struts → workarea                                                   */
/* ------------------------------------------------------------------ */

/* İleri bildirimler */
static void arrange_ws(LinisWM *wm, int ws, int interactive);

/* Tek dock penceresinin _NET_WM_STRUT değerlerini oku (4 CARDINAL). */
static void read_strut(LinisWM *wm, Window w, int *out)
{
	Atom type = None;
	int format = 0;
	unsigned long n = 0, after = 0;
	unsigned char *data = NULL;
	out[0] = 0; out[1] = 0; out[2] = 0; out[3] = 0;

	Atom strut = XInternAtom(wm->dpy, "_NET_WM_STRUT", False);
	if (XGetWindowProperty(wm->dpy, w, strut, 0, 4, False, XA_CARDINAL,
	                       &type, &format, &n, &after, &data) == Success &&
	    data && n >= 4) {
		long *p = (long *)data;
		out[0] = (int)p[0]; /* left  */
		out[1] = (int)p[1]; /* right */
		out[2] = (int)p[2]; /* top   */
		out[3] = (int)p[3]; /* bottom */
		if (data) XFree(data);
		return;
	}
	if (data) XFree(data);

	/* strut yoksa pencere geometrisinden tahmin et */
	XWindowAttributes a;
	if (XGetWindowAttributes(wm->dpy, w, &a)) {
		if (a.x <= 0 && a.width > 0)
			out[0] = a.width;
		if (a.x + a.width >= wm->sw - 1 && a.width > 0)
			out[1] = a.width;
		if (a.y <= 0 && a.height > 0)
			out[2] = a.height;
		if (a.y + a.height >= wm->sh - 1 && a.height > 0)
			out[3] = a.height;
	}
}

/* Dock penceresi mi? (herhangi bir root çocuğu için) */
static int is_dock_window(LinisWM *wm, Window w)
{
	Atom type = None;
	Atom typeAt = 0; int fmt = 0;
	unsigned long n = 0, after = 0;
	unsigned char *data = NULL;
	int dock = 0;
	if (XGetWindowProperty(wm->dpy, w, wm->net[NET_WM_WINDOW_TYPE], 0, 1,
	                       False, XA_ATOM, &typeAt, &fmt, &n, &after,
	                       &data) == Success && data && n >= 1)
		type = ((Atom *)data)[0];
	if (data) XFree(data);
	dock = (type == wm->net[NET_WM_WINDOW_TYPE_DOCK]);
	return dock;
}

/* Görünür tüm dock pencerelerinin strut'larından workarea hesapla. */
static void recompute_struts(LinisWM *wm)
{
	int l = 0, r = 0, t = 0, b = 0;

	/* managed dock (nadir) + root'taki override dock pencereleri */
	Window rr, *kids = NULL;
	unsigned int nk = 0;
	if (XQueryTree(wm->dpy, wm->root, &rr, &rr, &kids, &nk)) {
		for (unsigned int i = 0; i < nk; i++) {
			Window w = kids[i];
			XWindowAttributes a;
			if (!XGetWindowAttributes(wm->dpy, w, &a))
				continue;
			if (a.map_state != IsViewable)
				continue;
			if (!is_dock_window(wm, w))
				continue;
			int s[4];
			read_strut(wm, w, s);
			l = LMAX(l, s[0]);
			r = LMAX(r, s[1]);
			t = LMAX(t, s[2]);
			b = LMAX(b, s[3]);
		}
		if (kids) XFree(kids);
	}

	/* dock'lar strut bildirmediyse (henüz map olmadı) config gerekirse:
	 * burada ek bir varsayılan yok; panel autostart sırası strut'u kurar. */
	wm->strut_l = l; wm->strut_r = r;
	wm->strut_t = t; wm->strut_b = b;

	int x = l, y = t;
	int w = wm->sw - l - r;
	int h = wm->sh - t - b;
	if (w < 1) w = 1;
	if (h < 1) h = 1;
	wm->wa_x = x; wm->wa_y = y;
	wm->wa_w = w; wm->wa_h = h;

	ewmh_set_workarea(wm->dpy, wm->root, wm->net[NET_WORKAREA], x, y, w, h,
	                  wm->nws);
}

/* ------------------------------------------------------------------ */
/* EWMH güncellemeleri                                                 */
/* ------------------------------------------------------------------ */

static int client_count(LinisWM *wm)
{
	int n = 0;
	for (LinisClient *c = wm->clients; c; c = c->next)
		if (!c->ic)
			n++;
	return n;
}

static void update_client_list(LinisWM *wm)
{
	int n = 0;
	for (LinisClient *c = wm->clients; c; c = c->next)
		n++;
	Window *arr = xmalloc((size_t)LMAX(n, 1) * sizeof(Window));
	int i = 0;
	for (LinisClient *c = wm->clients; c; c = c->next)
		arr[i++] = c->w;
	ewmh_set_client_list(wm->dpy, wm->root, wm->net[NET_CLIENT_LIST], arr, n);
	ewmh_set_client_list(wm->dpy, wm->root, wm->net[NET_CLIENT_LIST_STACKING],
	                     arr, n);
	free(arr);
}

/* ------------------------------------------------------------------ */
/* Focus yönetimi                                                      */
/* ------------------------------------------------------------------ */

/* Dock pencerelerini yukarı al (fullscreen yoksa). */
static void raise_docks(LinisWM *wm)
{
	if (wm->focus && wm->focus->isfull)
		return;
	Window rr, *kids = NULL;
	unsigned int nk = 0;
	if (!XQueryTree(wm->dpy, wm->root, &rr, &rr, &kids, &nk))
		return;
	for (unsigned int i = 0; i < nk; i++) {
		Window w = kids[i];
		XWindowAttributes a;
		if (!XGetWindowAttributes(wm->dpy, w, &a))
			continue;
		if (a.override_redirect && is_dock_window(wm, w))
			XRaiseWindow(wm->dpy, w);
	}
	if (kids) XFree(kids);
}

static void set_focus(LinisWM *wm, LinisClient *c)
{
	Display *dpy = wm->dpy;
	wm->focus = c;
	if (!c) {
		ewmh_set_active_window(dpy, wm->root, wm->net[NET_ACTIVE_WINDOW],
		                       None);
		comp_set_active(wm->comp, None);
		XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
		raise_docks(wm);
		XFlush(dpy);
		return;
	}
	XWindowAttributes a;
	XGetWindowAttributes(dpy, c->w, &a);
	if (a.map_state == IsViewable) {
		/* ICCCM: input hint False ise WM_TAKE_FOCUS gönder, focus verme */
		XWMHints *h = XGetWMHints(dpy, c->w);
		int take = (h && (h->flags & InputHint) && !h->input);
		if (h)
			XFree(h);
		if (take) {
			icccm_take_focus(dpy, c->w, wm->ic_extra[0], wm->ic_extra[2],
			                 c->w);
		} else {
			XSetInputFocus(dpy, c->w, RevertToParent, CurrentTime);
		}
	}
	ewmh_set_active_window(dpy, wm->root, wm->net[NET_ACTIVE_WINDOW], c->w);
	comp_set_active(wm->comp, c->w);
	XRaiseWindow(dpy, c->w);
	raise_docks(wm);
	XFlush(dpy);
}

/* ------------------------------------------------------------------ */
/* Klavye yakalama                                                     */
/* ------------------------------------------------------------------ */

static void grab_key_combos(LinisWM *wm, unsigned keycode, unsigned mod)
{
	static const unsigned extra[] = { 0, LockMask, Mod2Mask,
		                              LockMask | Mod2Mask };
	for (int i = 0; i < 4; i++) {
		XGrabKey(wm->dpy, keycode, mod | extra[i], wm->root, False,
		         GrabModeAsync, GrabModeAsync);
		XGrabKey(wm->dpy, keycode, mod | extra[i] | ShiftMask, wm->root,
		         False, GrabModeAsync, GrabModeAsync);
	}
}

static void grab_binding(LinisWM *wm, KeySym sym, unsigned mod)
{
	Display *dpy = wm->dpy;
	KeyCode kc0 = XKeysymToKeycode(dpy, sym);
	if (!kc0)
		return;
	grab_key_combos(wm, kc0, mod);
	/* ikinci kaynak (örn. F1 hem de F1'nin ikinci eşlemesi) */
	KeySym sym2 = NoSymbol;
	if (sym >= 0x20 && sym < 0x7f) {
		/* büyük/küçük harf eşlemesi */
		if (sym >= 'A' && sym <= 'Z')
			sym2 = (KeySym)(sym + 32);
	}
	if (sym2 != NoSymbol) {
		KeyCode kc1 = XKeysymToKeycode(dpy, sym2);
		if (kc1 && kc1 != kc0)
			grab_key_combos(wm, kc1, mod);
	}
}

/* Super tuşunun tek basımını da yakala (launcher için). */
static void grab_mod_only(LinisWM *wm)
{
	Display *dpy = wm->dpy;
	KeySym modkeys[] = { XK_Super_L, XK_Super_R, XK_Hyper_L, XK_Hyper_R };
	for (int i = 0; i < 4; i++) {
		KeyCode kc = XKeysymToKeycode(dpy, modkeys[i]);
		if (!kc)
			continue;
		/* modsuz + lock varyasyonları */
		grab_key_combos(wm, kc, 0);
	}
}

/* ------------------------------------------------------------------ */
/* Komut dağıtıcı                                                      */
/* ------------------------------------------------------------------ */

static void cmd_handler(const char *arg, void *user);

static void ws_switch(LinisWM *wm, int target, int slide);

static void toggle_mode(LinisWM *wm);

static void spawn_launcher(LinisWM *wm)
{
	spawn(wm->cfg->launcher_cmd);
}

/* Fullscreen durumunu uygula/kaldır (EWMH state özelliğini de yazar). */
static void client_fullscreen(LinisWM *wm, LinisClient *c, int on)
{
	Display *dpy = wm->dpy;
	if (!c || c->isdock)
		return;
	if (on && !c->isfull) {
		c->ox = c->x; c->oy = c->y; c->ow = c->wd; c->oh = c->ht;
		c->isfull = 1;
		c->x = 0; c->y = 0; c->wd = wm->sw; c->ht = wm->sh;
		ewmh_set_state_flags(dpy, c->w, wm->net[NET_WM_STATE],
		                     wm->net[NET_WM_STATE_FULLSCREEN],
		                     wm->net[NET_WM_STATE_HIDDEN], 1, -1);
		XMoveResizeWindow(dpy, c->w, 0, 0, wm->sw, wm->sh);
		XRaiseWindow(dpy, c->w);
	} else if (!on && c->isfull) {
		c->isfull = 0;
		c->x = c->ox; c->y = c->oy; c->wd = c->ow; c->ht = c->oh;
		ewmh_set_state_flags(dpy, c->w, wm->net[NET_WM_STATE],
		                     wm->net[NET_WM_STATE_FULLSCREEN],
		                     wm->net[NET_WM_STATE_HIDDEN], 0, -1);
		if (c->f) {
			c->fx = c->x; c->fy = c->y; c->fw = c->wd; c->fh = c->ht;
			XMoveResizeWindow(dpy, c->w, c->x, c->y, c->wd, c->ht);
		}
	}
	XFlush(dpy);
}

/* "workspace:2", "spawn:firefox", "close", "toggle_tile"... */
static void dispatch_cmd(LinisWM *wm, const char *cmd)
{
	if (!cmd || !*cmd)
		return;
	if (str_startswith(cmd, "spawn:")) {
		spawn(cmd + 6);
		return;
	}
	if (strcmp(cmd, "close") == 0 || strcmp(cmd, "kill") == 0) {
		/* wm->focus'a WM_DELETE ClientMessage gönder (ICCCM) */
		LinisClient *fc = wm->focus;
		if (fc && !fc->isdock) {
			if (icccm_has_proto(wm->dpy, fc->w, wm->ic_extra[0],
			                    wm->ic_extra[1])) {
				XEvent e;
				memset(&e, 0, sizeof(e));
				e.xclient.type = ClientMessage;
				e.xclient.window = fc->w;
				e.xclient.message_type = wm->ic_extra[0]; /* WM_PROTOCOLS */
				e.xclient.format = 32;
				e.xclient.data.l[0] = (long)wm->ic_extra[1]; /* WM_DELETE */
				e.xclient.data.l[1] = CurrentTime;
				XSendEvent(wm->dpy, fc->w, False, NoEventMask, &e);
				XFlush(wm->dpy);
			} else {
				XKillClient(wm->dpy, fc->w);
			}
		}
		return;
	}
	if (strcmp(cmd, "toggle_tile") == 0 || strcmp(cmd, "tiling") == 0) {
		toggle_mode(wm);
		return;
	}
	if (strcmp(cmd, "float") == 0 || strcmp(cmd, "toggle_float") == 0) {
		LinisClient *c = wm->focus;
		if (c && !c->isdock && !c->isfull) {
			c->f = !c->f;
			if (c->f) {
				c->fx = c->x; c->fy = c->y; c->fw = c->wd; c->fh = c->ht;
			}
		}
		return;
	}
	if (strcmp(cmd, "fullscreen") == 0) {
		LinisClient *c = wm->focus;
		if (!c || c->isdock)
			return;
		client_fullscreen(wm, c, !c->isfull);
		if (!c->isfull && wm->wsmode[wm->curws] && !c->f)
			arrange_ws(wm, wm->curws, 1); /* tilinge geri dön */
		return;
	}
	if (strcmp(cmd, "monocle") == 0) {
		int ws = wm->curws;
		wm->wsmono[ws] = !wm->wsmono[ws];
		return;
	}
	if (str_startswith(cmd, "workspace:") || str_startswith(cmd, "ws:")) {
		const char *p = strchr(cmd, ':');
		int n = atoi(p + 1);
		if (n >= 1 && n <= wm->nws)
			ws_switch(wm, n - 1, 1);
		return;
	}
	if (str_startswith(cmd, "move_ws:") || str_startswith(cmd, "mws:")) {
		const char *p = strchr(cmd, ':');
		int n = atoi(p + 1);
		if (wm->focus && n >= 1 && n <= wm->nws) {
			LinisClient *c = wm->focus;
			c->ws = n - 1;
			ewmh_set_wm_desktop(wm->dpy, c->w, wm->net[NET_WM_DESKTOP],
			                    n - 1);
			if (n - 1 != wm->curws)
				XUnmapWindow(wm->dpy, c->w);
			update_client_list(wm);
		}
		return;
	}
	if (strcmp(cmd, "next_ws") == 0) {
		ws_switch(wm, (wm->curws + 1) % wm->nws, 1);
		return;
	}
	if (strcmp(cmd, "prev_ws") == 0) {
		ws_switch(wm, (wm->curws + wm->nws - 1) % wm->nws, -1);
		return;
	}
	if (strcmp(cmd, "focus_next") == 0 || strcmp(cmd, "focus_prev") == 0) {
		int dir = (cmd[0] == 'f' && cmd[6] == 'p') ? -1 : 1;
		/* görünür pencerelerden dizi kur */
		LinisClient *arr[128];
		int n = 0;
		for (LinisClient *c = wm->clients; c && n < 128; c = c->next) {
			if (c->ws != wm->curws || c->ic || c->isdock || c->isdesktop)
				continue;
			arr[n++] = c;
		}
		if (n == 0)
			return;
		int cur = 0;
		if (wm->focus)
			for (int i = 0; i < n; i++)
				if (arr[i] == wm->focus) { cur = i; break; }
		int tgt = ((cur + dir) % n + n) % n;
		if (arr[tgt] && arr[tgt] != wm->focus)
			set_focus(wm, arr[tgt]);
		return;
	}
	if (strcmp(cmd, "master_inc") == 0) {
		wm->cfg->master_ratio = LCLAMP(wm->cfg->master_ratio + 5, 30, 70);
		return;
	}
	if (strcmp(cmd, "master_dec") == 0) {
		wm->cfg->master_ratio = LCLAMP(wm->cfg->master_ratio - 5, 30, 70);
		return;
	}
	if (strcmp(cmd, "launcher") == 0) {
		spawn_launcher(wm);
		return;
	}
	if (strcmp(cmd, "boot_video") == 0) {
		bootvid_play(cmd, wm);
		return;
	}
	if (strcmp(cmd, "terminal") == 0) {
		spawn(wm->cfg->term_cmd);
		return;
	}
	if (strcmp(cmd, "filemgr") == 0) {
		spawn(wm->cfg->file_cmd);
		return;
	}
	if (strcmp(cmd, "restart") == 0) {
		wm->do_restart = 1;
		return;
	}
	if (strcmp(cmd, "quit") == 0) {
		wm->do_quit = 1;
		return;
	}
}

static void cmd_handler(const char *arg, void *user)
{
	LinisWM *wm = (LinisWM *)user;
	dispatch_cmd(wm, arg);
}

/* ------------------------------------------------------------------ */
/* Pencere yönetimi                                                    */
/* ------------------------------------------------------------------ */

static void client_ungrab_buttons(LinisWM *wm, Window w)
{
	Display *dpy = wm->dpy;
	XUngrabButton(dpy, AnyButton, AnyModifier, w);
}

static void client_grab_buttons(LinisWM *wm, Window w)
{
	Display *dpy = wm->dpy;
	/* Mod4 + LMB: taşı, Mod4 + RMB: boyutlandır */
	XGrabButton(dpy, Button1, MOD_MOVE, w, False,
	            ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
	            GrabModeAsync, GrabModeAsync, None, None);
	XGrabButton(dpy, Button3, MOD_MOVE, w, False,
	            ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
	            GrabModeAsync, GrabModeAsync, None, None);
}

/* Pencere geometrisini uygula (border 0, girdi olayı seçimi). */
static void setup_window_events(LinisWM *wm, Window w)
{
	XSelectInput(wm->dpy, w,
	             StructureNotifyMask | PropertyChangeMask |
	             EnterWindowMask | FocusChangeMask);
}

static void client_free(LinisWM *wm, LinisClient *c)
{
	client_ungrab_buttons(wm, c->w);
	if (c->name)
		free(c->name);
	free(c);
}

/* Pencereyi göster + compositor'e kaydet (+ açılma animasyonu). */
static void client_show(LinisWM *wm, LinisClient *c, int animate)
{
	XMapWindow(wm->dpy, c->w);
	comp_win_map(wm->comp, c->w);
	if (animate && wm->cfg->comp.animations)
		comp_anim_open(wm->comp, c->w);
}

/* Bir pencereyi yönet (MapRequest geldi). */
static void manage_window(LinisWM *wm, Window w)
{
	Display *dpy = wm->dpy;
	if (find_client(wm, w))
		return;

	XWindowAttributes a;
	if (!XGetWindowAttributes(dpy, w, &a))
		return;
	if (a.override_redirect)
		return;
	if (a.class == InputOnly)
		return;

	/* yönetilemez türler: dock zaten ayrıca strut'u işlenir */
	LinisClient *c = xcalloc(1, sizeof(*c));
	c->w = w;
	c->x = a.x; c->y = a.y; c->wd = a.width; c->ht = a.height;
	c->idx = 0;

	Atom type = 0;
	Atom typeAt = 0; int fmt = 0; unsigned long n = 0, after = 0;
	unsigned char *data = NULL;
	if (XGetWindowProperty(dpy, w, wm->net[NET_WM_WINDOW_TYPE], 0, 1, False,
	                       XA_ATOM, &typeAt, &fmt, &n, &after, &data) ==
	        Success &&
	    data && n >= 1)
		type = ((Atom *)data)[0];
	if (data) XFree(data);

	c->isdock = (type == wm->net[NET_WM_WINDOW_TYPE_DOCK]);
	c->isdialog = (type == wm->net[NET_WM_WINDOW_TYPE_DIALOG]);
	c->isdesktop = (type == wm->net[NET_WM_WINDOW_TYPE_DESKTOP]);

	int ws = wm->curws;
	int gd = get_wm_desktop(wm, w);
	if (gd >= 0 && gd < wm->nws)
		ws = gd;
	c->ws = ws;

	/* dialog/utility → otomatik floating */
	if (c->isdialog)
		c->f = 1;
	if (c->isdesktop) {
		c->f = 1;
		c->ws = wm->curws;
	}

	char *nm = ewmh_get_name(dpy, w);
	c->name = nm ? nm : xstrdup("");

	/* hint geometrisini uygula (eğer boyutlar anlamlıysa) */
	XSizeHints sh;
	long dummy;
	if (XGetWMNormalHints(dpy, w, &sh, &dummy)) {
		if (c->wd < 40 && (sh.flags & PMinSize) && sh.min_width > 0)
			c->wd = sh.min_width;
		if (c->ht < 40 && (sh.flags & PMinSize) && sh.min_height > 0)
			c->ht = sh.min_height;
	}

	/* floating varsayılan geometri: workarea içinde kademeli yerleştir */
	if (c->f && !c->isdock) {
		if (c->wd < 40 || c->ht < 40) {
			c->wd = LMIN(900, wm->wa_w * 2 / 3);
			c->ht = LMIN(600, wm->wa_h * 2 / 3);
		}
		/* merkezleme */
		c->x = wm->wa_x + (wm->wa_w - c->wd) / 2;
		c->y = wm->wa_y + (wm->wa_h - c->ht) / 2;
		if (c->x < wm->wa_x) c->x = wm->wa_x;
		if (c->y < wm->wa_y) c->y = wm->wa_y;
	}
	if (c->isdock) {
		/* dock geometrisini koru; configure isteklerine izin ver */
		c->f = 1;
	}

	/* XAddToSaveSet + olay maskeleri */
	XAddToSaveSet(dpy, w);
	client_grab_buttons(wm, w);
	setup_window_events(wm, w);

	/* listeye ekle (idx'ler yeniden) */
	int maxidx = -1;
	for (LinisClient *q = wm->clients; q; q = q->next)
		maxidx = LMAX(maxidx, q->idx);
	c->idx = maxidx + 1;
	c->next = wm->clients;
	wm->clients = c;

	/* pencereyi istenen geometriye çekip göster */
	XMoveResizeWindow(dpy, w, c->x, c->y, c->wd, c->ht);
	client_show(wm, c, 1);
	XFlush(dpy);

	update_client_list(wm);
	recompute_struts(wm);

	if (c->isdock) {
		XRaiseWindow(dpy, w);
		XFlush(dpy);
		return;
	}

	/* yeni pencerede bir client yoksa ya da ilk normal client ise odaklan */
	if (!wm->focus)
		set_focus(wm, c);
	else if (c->ws == wm->curws && wm->wsmode[wm->curws])
		set_focus(wm, c);

	if (c->ws == wm->curws)
		arrange_ws(wm, wm->curws, 1);
}

static void unmanage_window(LinisWM *wm, Window w)
{
	Display *dpy = wm->dpy;
	LinisClient **pp = &wm->clients;
	LinisClient *c = NULL;
	while (*pp) {
		if ((*pp)->w == w) {
			c = *pp;
			*pp = c->next;
			break;
		}
		pp = &(*pp)->next;
	}
	if (!c)
		return;
	if (wm->focus == c)
		wm->focus = NULL;
	client_free(wm, c);
	update_client_list(wm);
	recompute_struts(wm);
	if (!wm->focus) {
		/* yeni odak: görünür ilk client */
		for (LinisClient *q = wm->clients; q; q = q->next) {
			if (q->ws == wm->curws && !q->ic && !q->isdock) {
				set_focus(wm, q);
				break;
			}
		}
		if (!wm->focus)
			set_focus(wm, NULL);
	}
	arrange_ws(wm, wm->curws, 1);
}

/* ------------------------------------------------------------------ */
/* Layout / tiling                                                     */
/* ------------------------------------------------------------------ */

static void set_geom(LinisWM *wm, LinisClient *c, int x, int y, int w, int h)
{
	c->x = x; c->y = y; c->wd = w; c->ht = h;
	if (c->isfull)
		return;
	XMoveResizeWindow(wm->dpy, c->w, x, y, w, h);
}

/* Belirli bir workspace'teki düzeni yeniden kur. */
static void arrange_ws(LinisWM *wm, int ws, int interactive)
{
	Display *dpy = wm->dpy;
	LinisClient *first = NULL;

	/* visible clients of ws */
	for (LinisClient *c = wm->clients; c; c = c->next) {
		if (c->ws == ws && c->ic)
			continue;
		if (c->ws != ws)
			continue;
		/* fullscreen: her zaman tam ekran */
		if (c->isfull) {
			if (ws == wm->curws)
				set_geom(wm, c, 0, 0, wm->sw, wm->sh);
			continue;
		}
		if (!first)
			first = c;
	}
	if (!first && !interactive)
		return;

	int mono = wm->wsmono[ws];
	int tiling = wm->wsmode[ws] && !mono;

	/* monocle: odak dışındaki pencereler haritada kalsın, odağı en üste
	 * al — görsel tek pencere efekti. */
	if (mono) {
		LinisClient *show = wm->focus;
		if (show && show->ws == ws && !show->ic && !show->isdock) {
			/* diğer görünür pencereleri gizle */
			for (LinisClient *c = wm->clients; c; c = c->next) {
				if (c->ws != ws || c->ic || c->isdock)
					continue;
				if (c == show)
					continue;
				XLowerWindow(dpy, c->w);
			}
			XRaiseWindow(dpy, show->w);
			set_geom(wm, show, wm->wa_x, wm->wa_y, wm->wa_w, wm->wa_h);
		}
		XFlush(dpy);
		return;
	}

	/* tiling dışındakiler (floating) yerinde kalır */
	if (!tiling) {
		/* kademeli serbest pencereleri hizalı tutmak isteğe bağlı */
		return;
	}

	/* autotile pencereleri topla (floating override yoksa) */
	LinisClient *tiles[64];
	int n = 0;
	for (LinisClient *c = wm->clients; c; c = c->next) {
		if (c->ws != ws || c->ic || c->isfull)
			continue;
		if (c->f || c->isdock || c->isdesktop)
			continue;
		if (n < 64)
			tiles[n++] = c;
	}
	if (n == 0)
		return;

	int gw = wm->cfg->outer_gap;
	int ig = wm->cfg->inner_gap;
	int ax = wm->wa_x + gw, ay = wm->wa_y + gw;
	int aw = wm->wa_w - 2 * gw, ah = wm->wa_h - 2 * gw;
	if (aw < 0) aw = 0;
	if (ah < 0) ah = 0;

	int ratio = LCLAMP(wm->cfg->master_ratio, 30, 70);
	if (n == 1) {
		set_geom(wm, tiles[0], ax, ay, aw, ah);
	} else {
		int mw = aw * ratio / 100;
		int sw_ = aw - mw - ig;
		set_geom(wm, tiles[0], ax, ay, mw, ah);
		int stack_h = ah - (n - 2) * ig;
		int rowh = stack_h / (n - 1);
		int ry = ay;
		for (int i = 1; i < n; i++) {
			int hh = (i == n - 1) ? (ay + ah - ry) : rowh;
			set_geom(wm, tiles[i], ax + mw + ig, ry, sw_, hh);
			ry += hh + ig;
		}
	}
	XFlush(dpy);
	(void)first;
}

/* workspace modunu değiştir */
static void toggle_mode(LinisWM *wm)
{
	int ws = wm->curws;
	wm->wsmode[ws] = !wm->wsmode[ws];
	if (!wm->wsmode[ws]) {
		/* floating moda dön: her autotile pencereyi serbest bırak */
		for (LinisClient *c = wm->clients; c; c = c->next) {
			if (c->ws != ws || c->ic || c->f || c->isfull)
				continue;
			c->f = 1;
			c->fx = c->x; c->fy = c->y; c->fw = c->wd; c->fh = c->ht;
		}
	} else {
		/* tiling mod: mevcut tüm normal pencereler döşensin */
		for (LinisClient *c = wm->clients; c; c = c->next) {
			if (c->ws != ws || c->ic || c->isdock || c->isdesktop)
				continue;
			if (c->f && !c->isfull)
				c->f = 0; /* yeniden oto-döşeme */
		}
	}
	arrange_ws(wm, ws, 1);
}

/* ------------------------------------------------------------------ */
/* Workspace geçişi                                                    */
/* ------------------------------------------------------------------ */

static void ws_switch(LinisWM *wm, int target, int slide)
{
	if (target == wm->curws)
		return;
	Display *dpy = wm->dpy;
	int dir = (target > wm->curws) ? 1 : -1;
	if (slide)
		comp_anim_slide(wm->comp, dir);

	/* mevcut ws kapat */
	for (LinisClient *c = wm->clients; c; c = c->next) {
		if (c->ws == wm->curws && !c->ic && !c->isdock)
			XUnmapWindow(dpy, c->w);
	}
	wm->curws = target;
	/* hedef ws aç */
	LinisClient *focus = NULL;
	for (LinisClient *c = wm->clients; c; c = c->next) {
		if (c->ws != target || c->ic || c->isdock)
			continue;
		client_show(wm, c, 0);
		if (!focus)
			focus = c;
	}
	ewmh_set_current_desktop(dpy, wm->root, wm->net[NET_CURRENT_DESKTOP],
	                         target);
	arrange_ws(wm, target, 1);
	if (focus) {
		client_show(wm, focus, 1);
		set_focus(wm, focus);
	} else {
		set_focus(wm, NULL);
	}
	XFlush(dpy);
	(void)slide;
}

/* ------------------------------------------------------------------ */
/* İkonlaştırma (minimize)                                             */
/* ------------------------------------------------------------------ */

static void iconify_client(LinisWM *wm, LinisClient *c)
{
	if (!c || c->isdock)
		return;
	c->ic = 1;
	if (wm->focus == c) {
		/* yeni odağı bul */
		LinisClient *nf = NULL;
		for (LinisClient *q = wm->clients; q; q = q->next) {
			if (q->ws == wm->curws && !q->ic && !q->isdock && q != c) {
				nf = q;
				break;
			}
		}
		set_focus(wm, nf);
	}
	/* tiling penceresi: anında; floating: animasyonlu */
	if (!c->f) {
		XUnmapWindow(wm->dpy, c->w);
		arrange_ws(wm, wm->curws, 1);
	} else {
		comp_anim_minimize(wm->comp, c->w);
	}
	update_client_list(wm);
	XFlush(wm->dpy);
}

static void uniconify_client(LinisWM *wm, LinisClient *c)
{
	if (!c->ic)
		return;
	c->ic = 0;
	if (c->ws != wm->curws) {
		c->ws = wm->curws;
		ewmh_set_wm_desktop(wm->dpy, c->w, wm->net[NET_WM_DESKTOP],
		                    wm->curws);
	}
	client_show(wm, c, 1);
	arrange_ws(wm, wm->curws, 1);
	set_focus(wm, c);
	update_client_list(wm);
	XFlush(wm->dpy);
}

/* ------------------------------------------------------------------ */
/* Fare etkileşimi                                                     */
/* ------------------------------------------------------------------ */

static void start_drag(LinisWM *wm, LinisClient *c, XButtonEvent *ev, int mode)
{
	/* tiling'deysen farenin yakaladığı pencereyi float yap */
	if (wm->wsmode[wm->curws] && !c->f && !c->isfull && !c->isdock) {
		c->f = 1;
		c->fx = c->x; c->fy = c->y; c->fw = c->wd; c->fh = c->ht;
		arrange_ws(wm, wm->curws, 1);
	}
	wm->drag = c;
	wm->dragmode = mode;
	wm->dragx = ev->x_root;
	wm->dragy = ev->y_root;
	wm->dwin_x = c->x; wm->dwin_y = c->y;
	wm->dwin_w = c->wd; wm->dwin_h = c->ht;
	XGrabPointer(wm->dpy, wm->root, False,
	             ButtonReleaseMask | PointerMotionMask, GrabModeAsync,
	             GrabModeAsync, None, None, CurrentTime);
}

static void end_drag(LinisWM *wm)
{
	wm->drag = NULL;
	XUngrabPointer(wm->dpy, CurrentTime);
}

/* ------------------------------------------------------------------ */
/* Olay işleyici                                                       */
/* ------------------------------------------------------------------ */

static void on_maprequest(LinisWM *wm, XMapRequestEvent *e)
{
	manage_window(wm, e->window);
}

static void on_configure_request(LinisWM *wm, XConfigureRequestEvent *e)
{
	Display *dpy = wm->dpy;
	LinisClient *c = find_client(wm, e->window);
	if (!c)
		return;
	if (c->isfull || c->isdock)
		return;
	XSizeHints sh;
	long dummy;
	int minw = 0, minh = 0, maxw = 0, maxh = 0;
	if (XGetWMNormalHints(dpy, c->w, &sh, &dummy)) {
		if (sh.flags & PMinSize) { minw = sh.min_width; minh = sh.min_height; }
		if (sh.flags & PMaxSize) { maxw = sh.max_width; maxh = sh.max_height; }
	}
	int x = (e->value_mask & CWX) ? e->x : c->x;
	int y = (e->value_mask & CWY) ? e->y : c->y;
	int w = (e->value_mask & CWWidth) ? e->width : c->wd;
	int h = (e->value_mask & CWHeight) ? e->height : c->ht;
	if (w < minw) w = minw;
	if (h < minh) h = minh;
	if (maxw && w > maxw) w = maxw;
	if (maxh && h > maxh) h = maxh;

	if (wm->wsmode[wm->curws] && !c->f) {
		/* tiling: pencere talebi yerine düzeni koru */
		return;
	}
	if (c->f && !c->ic && c->ws == wm->curws) {
		c->x = x; c->y = y; c->wd = w; c->ht = h;
		c->fx = x; c->fy = y; c->fw = w; c->fh = h;
		XMoveResizeWindow(dpy, c->w, x, y, w, h);
	} else if (c->ic) {
		/* simge durumundayken boyutu hatırla */
		c->wd = w; c->ht = h;
	}
}

static void on_mapnotify(LinisWM *wm, XMapEvent *e)
{
	if (e->event != wm->root)
		return;
	/* OR pencereleri dahil her görünen pencere compositor'e kayıtlı */
	comp_win_map(wm->comp, e->window);

	/* dock (panel) → strut bildirimlerini dinle ve düzeni güncelle */
	if (is_dock_window(wm, e->window)) {
		XSelectInput(wm->dpy, e->window,
		             PropertyChangeMask | StructureNotifyMask);
		recompute_struts(wm);
		arrange_ws(wm, wm->curws, 1);
	}
}

static void on_unmapnotify(LinisWM *wm, XUnmapEvent *e)
{
	if (e->event != wm->root)
		return;
	comp_win_unmap(wm->comp, e->window);

	/* normal (managed) pencerelerin kendi isteğiyle kapanması */
	LinisClient *c = find_client(wm, e->window);
	if (c && !e->send_event && !c->ic) {
		/* client kendi kendini kapattı */
		unmanage_window(wm, e->window);
	}
}

static void on_destroynotify(LinisWM *wm, XDestroyWindowEvent *e)
{
	comp_win_unmap(wm->comp, e->window);
	LinisClient *c = find_client(wm, e->window);
	if (c)
		unmanage_window(wm, e->window);
}

static void on_propertynotify(LinisWM *wm, XPropertyEvent *e)
{
	if (e->window == wm->root)
		return;
	Atom s = XInternAtom(wm->dpy, "_NET_WM_STRUT", False);
	Atom sp = XInternAtom(wm->dpy, "_NET_WM_STRUT_PARTIAL", False);
	if (e->atom == s || e->atom == sp) {
		recompute_struts(wm);
		arrange_ws(wm, wm->curws, 1);
		return;
	}
	LinisClient *c = find_client(wm, e->window);
	if (!c)
		return;
	/* isim değişti */
	if (e->atom == XA_WM_NAME ||
	    e->atom == XInternAtom(wm->dpy, "_NET_WM_NAME", False)) {
		char *nm = ewmh_get_name(wm->dpy, c->w);
		free(c->name);
		c->name = nm ? nm : xstrdup("");
	}
}

static void on_enter(LinisWM *wm, XCrossingEvent *e)
{
	/* sloppy focus */
	LinisClient *c = find_client(wm, e->window);
	if (c && !c->isdock && !c->ic && c->ws == wm->curws && !c->isdesktop)
		if (wm->focus != c)
			set_focus(wm, c);
}

static void on_button(LinisWM *wm, XButtonEvent *e)
{
	LinisClient *c = find_client(wm, e->window);
	if (!c)
		return;
	if (wm->focus != c)
		set_focus(wm, c);
	if (e->button == Button1 && e->state & MOD_MOVE) {
		start_drag(wm, c, e, 0);
	} else if (e->button == Button3 && e->state & MOD_MOVE) {
		start_drag(wm, c, e, 1);
	}
}

static void on_motion(LinisWM *wm, XMotionEvent *e)
{
	LinisClient *c = wm->drag;
	if (!c)
		return;
	int dx = e->x_root - wm->dragx;
	int dy = e->y_root - wm->dragy;
	if (wm->dragmode == 0) {
		int nx = wm->dwin_x + dx;
		int ny = wm->dwin_y + dy;
		c->x = nx; c->y = ny;
		c->fx = nx; c->fy = ny;
		XMoveWindow(wm->dpy, c->w, nx, ny);
	} else {
		int nw = wm->dwin_w + dx;
		int nh = wm->dwin_h + dy;
		if (nw < 80) nw = 80;
		if (nh < 60) nh = 60;
		c->wd = nw; c->ht = nh;
		c->fw = nw; c->fh = nh;
		XResizeWindow(wm->dpy, c->w, nw, nh);
	}
	XFlush(wm->dpy);
}

static void on_buttonrelease(LinisWM *wm)
{
	end_drag(wm);
}

static void on_clientmessage(LinisWM *wm, XClientMessageEvent *e)
{
	/* mpv benzeri uygulamaların _NET_WM_STATE tam ekran isteği */
	if (e->message_type == wm->net[NET_WM_STATE] && e->format == 32) {
		long action = e->data.l[0]; /* 0 REMOVE 1 ADD 2 TOGGLE */
		LinisClient *c = find_client(wm, e->window);
		if (!c || c->isdock)
			return;
		for (int i = 1; i <= 2; i++) {
			Atom a = (Atom)e->data.l[i];
			if (!a)
				continue;
			if (a == wm->net[NET_WM_STATE_FULLSCREEN]) {
				int want = action == 1 ? 1
				         : (action == 2 ? !c->isfull : 0);
				if (want != c->isfull) {
					client_fullscreen(wm, c, want);
					if (!want && wm->wsmode[wm->curws] && !c->f)
						arrange_ws(wm, wm->curws, 1);
				}
			} else if (a == wm->net[NET_WM_STATE_HIDDEN] && action == 1) {
				if (!c->ic)
					iconify_client(wm, c);
			}
		}
		return;
	}
	/* EWMH istekleri (panel/launcher bu mesajları gönderir) */
	if (e->message_type == wm->net[NET_ACTIVE_WINDOW] && e->format == 32) {
		LinisClient *c = find_client(wm, (Window)e->data.l[0]);
		if (c) {
			if (c->ic)
				uniconify_client(wm, c);
			if (c->ws != wm->curws) {
				ws_switch(wm, c->ws, 1);
				set_focus(wm, find_client(wm, c->w));
			} else {
				set_focus(wm, c);
			}
		}
	} else if (e->message_type == wm->net[NET_CURRENT_DESKTOP] &&
	           e->format == 32) {
		int n = (int)e->data.l[0];
		if (n >= 0 && n < wm->nws)
			ws_switch(wm, n, 1);
	} else if (e->message_type == wm->net[NET_CLOSE_WINDOW]) {
		LinisClient *c = find_client(wm, e->window);
		if (c && !c->isdock) {
			/* odak c ise close yoksa c'yi odakla kapat */
			LinisClient *prev = wm->focus;
			set_focus(wm, c);
			dispatch_cmd(wm, "close");
			if (prev && prev != c && find_client(wm, prev->w))
				set_focus(wm, prev);
		}
	} else if (e->message_type == wm->net[NET_WM_DESKTOP] && e->format == 32) {
		LinisClient *c = find_client(wm, e->window);
		int n = (int)e->data.l[0];
		if (c && n >= 0 && n < wm->nws) {
			c->ws = n;
			ewmh_set_wm_desktop(wm->dpy, c->w, wm->net[NET_WM_DESKTOP], n);
			if (n != wm->curws)
				XUnmapWindow(wm->dpy, c->w);
			else if (c->ic)
				uniconify_client(wm, c);
			update_client_list(wm);
			arrange_ws(wm, wm->curws, 1);
		}
	}
}

static void on_configurenotify(LinisWM *wm, XConfigureEvent *e)
{
	if (e->event == wm->root) {
		/* ekran boyutu değişebilir */
		XWindowAttributes a;
		if (XGetWindowAttributes(wm->dpy, wm->root, &a)) {
			wm->sw = a.width;
			wm->sh = a.height;
			recompute_struts(wm);
			arrange_ws(wm, wm->curws, 1);
		}
	} else {
		LinisClient *c = find_client(wm, e->window);
		if (c) {
			c->x = e->x; c->y = e->y; c->wd = e->width; c->ht = e->height;
		}
	}
}

static void on_keypress(LinisWM *wm, XKeyEvent *e)
{
	kb_handle(wm->kb, e);
}

static void on_keyrelease(LinisWM *wm, XKeyEvent *e)
{
	kb_handle(wm->kb, e);
}

int wm_handle_event(LinisWM *wm, const XEvent *ev)
{
	switch (ev->type) {
	case MapRequest:
		on_maprequest(wm, &ev->xmaprequest);
		return 1;
	case ConfigureRequest:
		on_configure_request(wm, &ev->xconfigurerequest);
		return 1;
	case MapNotify:
		on_mapnotify(wm, &ev->xmap);
		return 1;
	case UnmapNotify:
		on_unmapnotify(wm, &ev->xunmap);
		return 1;
	case DestroyNotify:
		on_destroynotify(wm, &ev->xdestroywindow);
		return 1;
	case PropertyNotify:
		on_propertynotify(wm, &ev->xproperty);
		return 1;
	case EnterNotify:
		on_enter(wm, &ev->xcrossing);
		return 1;
	case ButtonPress:
		on_button(wm, &ev->xbutton);
		return 1;
	case ButtonRelease:
		on_buttonrelease(wm);
		return 1;
	case MotionNotify:
		on_motion(wm, &ev->xmotion);
		return 1;
	case KeyPress:
		on_keypress(wm, &ev->xkey);
		return 1;
	case KeyRelease:
		on_keyrelease(wm, &ev->xkey);
		return 1;
	case ClientMessage:
		on_clientmessage(wm, &ev->xclient);
		return 1;
	case ConfigureNotify:
		on_configurenotify(wm, &ev->xconfigure);
		return 1;
	case MappingNotify:
		XRefreshKeyboardMapping(&ev->xmapping);
		return 1;
	default:
		break;
	}
	/* DamageNotify vs. compositor'e */
	if (comp_handle_event(wm->comp, ev))
		return 1;
	return 0;
}

int wm_loop_tick(LinisWM *wm)
{
	/* Super tek basımı launcher (release tabanlı) + zaman aşımı yedeği */
	kb_tick(wm->kb);
	return comp_frame(wm->comp);
}

int wm_timeout_ms(LinisWM *wm)
{
	return comp_timeout_ms(wm->comp);
}

/* ------------------------------------------------------------------ */
/* Yerleşik varsayılan kısayollar (keybinds.conf yoksa)               */
/* ------------------------------------------------------------------ */

static const struct {
	const char *key;
	const char *cmd;
} default_binds[] = {
	{ "mod4+t",            "toggle_tile"     },
	{ "mod4+q",            "close"           },
	{ "mod4+f",            "fullscreen"      },
	{ "mod4+m",            "monocle"         },
	{ "mod4+space",        "float"           },
	{ "mod4+h",            "master_dec"      },
	{ "mod4+l",            "master_inc"      },
	{ "mod4+j",            "focus_next"      },
	{ "mod4+k",            "focus_prev"      },
	{ "mod4+1",            "workspace:1"     },
	{ "mod4+2",            "workspace:2"     },
	{ "mod4+3",            "workspace:3"     },
	{ "mod4+4",            "workspace:4"     },
	{ "mod4+shift+1",      "move_ws:1"       },
	{ "mod4+shift+2",      "move_ws:2"       },
	{ "mod4+shift+3",      "move_ws:3"       },
	{ "mod4+shift+4",      "move_ws:4"       },
	{ "mod4+shift+q",      "quit"            },
	{ "mod4+shift+r",      "restart"         },
	{ "control+alt+t",     "terminal"        },
	{ "control+alt+f",     "filemgr"         },
	{ "mod4+p",            "launcher"        },
	{ "mod4+v",            "boot_video"      }, /* açılış videosu */
};

/* ------------------------------------------------------------------ */
/* Sinyaller                                                           */
/* ------------------------------------------------------------------ */

static void sig_quit(int s)  { (void)s; if (g_wm) g_wm->do_quit = 1; }
static void sig_restart(int s){ (void)s; if (g_wm) g_wm->do_restart = 1; }
static void sig_wall(int s) { (void)s; if (g_wm) comp_wall_recapture(g_wm->comp); }

/* ------------------------------------------------------------------ */
/* EWMH kök özellikleri                                                */
/* ------------------------------------------------------------------ */

static void ewmh_root_init(LinisWM *wm)
{
	Display *dpy = wm->dpy;
	Window root = wm->root;

	/* supporting wm check window */
	wm->check = XCreateSimpleWindow(dpy, root, -100, -100, 1, 1, 0, 0, 0);
	ewmh_set_wm_check(dpy, root, wm->check);
	XChangeProperty(dpy, wm->check, wm->net[NET_SUPPORTING_WM_CHECK],
	                XA_WINDOW, 32, PropModeReplace,
	                (const unsigned char *)&wm->check, 1);
	ewmh_set_supported(dpy, root, wm->net);

	ewmh_set_num_desktops(dpy, root, wm->net[NET_NUMBER_OF_DESKTOPS],
	                      wm->nws);
	static const char *names[] = { "1", "2", "3", "4", "5", "6", "7", "8",
		                           "9" };
	ewmh_set_desktop_names(dpy, root, wm->net[NET_DESKTOP_NAMES], names,
	                       wm->nws);
	ewmh_set_current_desktop(dpy, root, wm->net[NET_CURRENT_DESKTOP], 0);
	XChangeProperty(dpy, root, XInternAtom(dpy, "_NET_DESKTOP_VIEWPORT", False),
	                XA_CARDINAL, 32, PropModeReplace, NULL, 0);
	recompute_struts(wm);
	XMapWindow(dpy, wm->check);
}

/* ------------------------------------------------------------------ */
/* Açılış kapanış                                                      */
/* ------------------------------------------------------------------ */

static void grab_all_keys(LinisWM *wm)
{
	/* önce eski grab'ları temizle */
	XUngrabKey(wm->dpy, AnyKey, AnyModifier, wm->root);

	/* her kayıtlı kombinasyonu yakala */
	for (KBinding *b = kb_bindings(wm->kb); b; b = b->next)
		grab_binding(wm, b->sym, b->mod);
	grab_mod_only(wm);
}

static void load_keybinds(LinisWM *wm)
{
	LKeybinds *k = kb_new();
	kb_watch_mod(k, Mod4Mask, 400, cmd_handler, "launcher");

	char *f = linis_find_file("keybinds.conf");
	if (f && kb_load_file(k, f, cmd_handler, NULL, wm) > 0) {
		free(f);
		wm->have_keyconf = 1;
	} else {
		if (f) free(f);
		for (size_t i = 0; i < LARR_LEN(default_binds); i++)
			kb_add_str(k, default_binds[i].cmd, default_binds[i].key,
			           cmd_handler, default_binds[i].cmd, wm);
	}
	wm->kb = k;
}

LinisWM *wm_create(Display *dpy, const LConfig *cfg)
{
	LinisWM *wm = xcalloc(1, sizeof(*wm));
	g_wm = wm;
	wm->dpy = dpy;
	wm->scr = DefaultScreen(dpy);
	wm->root = RootWindow(dpy, wm->scr);
	wm->cfg = (LConfig *)cfg;
	{
		Screen *s = ScreenOfDisplay(dpy, wm->scr);
		wm->sw = WidthOfScreen(s);
		wm->sh = HeightOfScreen(s);
	}
	wm->nws = LCLAMP(cfg->workspace_count, 1, MAXWS);
	wm->curws = 0;

	/* başka WM var mı? SubstructureRedirect zaten takılıysa BadAccess */
	XSetErrorHandler(xerr_ignore);
	XSelectInput(dpy, wm->root, SubstructureRedirectMask |
	                            SubstructureNotifyMask | PropertyChangeMask |
	                            ButtonPressMask);
	XSync(dpy, False);

	/* ICCCM + EWMH atomları */
	ewmh_init(dpy, wm->net);
	wm->ic_extra[0] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wm->ic_extra[1] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wm->ic_extra[2] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);

	ewmh_root_init(wm);

	/* config'den strut yerine varsayılan alan (panel erken başlamadıysa) */
	recompute_struts(wm);

	/* compositor */
	wm->comp = comp_create(dpy, wm->scr, cfg);

	/* kısayollar */
	load_keybinds(wm);
	grab_all_keys(wm);

	/* mevcut (zaten açık) pencereleri yönet */
	Window rr, *kids = NULL;
	unsigned int nk = 0;
	if (XQueryTree(dpy, wm->root, &rr, &rr, &kids, &nk)) {
		for (unsigned int i = 0; i < nk; i++) {
			XWindowAttributes a;
			if (XGetWindowAttributes(dpy, kids[i], &a) && a.override_redirect)
				comp_win_map(wm->comp, kids[i]);
			else if (kids[i] != wm->check)
				manage_window(wm, kids[i]);
		}
		if (kids) XFree(kids);
	}
	update_client_list(wm);

	signal(SIGCHLD, SIG_IGN);
	signal(SIGTERM, sig_quit);
	signal(SIGINT, sig_quit);
	signal(SIGUSR1, sig_wall);
	signal(SIGHUP, sig_restart);

	arrange_ws(wm, 0, 1);
	XSync(dpy, False);
	return wm;
}

void wm_restart(LinisWM *wm)
{
	wm->do_restart = 1;
}

void wm_quit(LinisWM *wm)
{
	wm->do_quit = 1;
}

void wm_reload_wallpaper(LinisWM *wm)
{
	comp_wall_recapture(wm->comp);
}

void wm_destroy(LinisWM *wm)
{
	Display *dpy = wm->dpy;
	/* klavyeleri bırak */
	XUngrabKey(dpy, AnyKey, AnyModifier, wm->root);
	/* tüm pencereleri serbest bırak */
	LinisClient *c = wm->clients;
	while (c) {
		LinisClient *n = c->next;
		XRemoveFromSaveSet(dpy, c->w);
		client_free(wm, c);
		c = n;
	}
	wm->clients = NULL;
	if (wm->comp)
		comp_destroy(wm->comp);
	if (wm->kb)
		kb_free(wm->kb);
	XDeleteProperty(dpy, wm->root, wm->net[NET_SUPPORTING_WM_CHECK]);
	XDestroyWindow(dpy, wm->check);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XSync(dpy, False);
	g_wm = NULL;
	free(wm);
}

/* ------------------------------------------------------------------ */
/* Dış erişimciler                                                     */
/* ------------------------------------------------------------------ */

int wm_wants_quit(LinisWM *wm) { return wm->do_quit; }
int wm_wants_restart(LinisWM *wm) { return wm->do_restart; }
