/*
 * linis_ewmh.c — EWMH/ICCCM uyumluluk katmanı.
 * Not: "atom isimleri = atom" dizisi üzerinden gider, XInternAtom
 * çağrılarını tek noktada toplar (LINIS_DESIGN.md §3.4, §3.5).
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "linis_ewmh.h"
#include "linis_util.h"

static const char *const net_names[] = {
	"_NET_SUPPORTED", "_NET_SUPPORTING_WM_CHECK", "_NET_CLIENT_LIST",
	"_NET_CLIENT_LIST_STACKING", "_NET_NUMBER_OF_DESKTOPS",
	"_NET_DESKTOP_GEOMETRY", "_NET_DESKTOP_VIEWPORT", "_NET_CURRENT_DESKTOP",
	"_NET_DESKTOP_NAMES", "_NET_ACTIVE_WINDOW", "_NET_WORKAREA",
	"_NET_WM_NAME", "_NET_WM_VISIBLE_NAME", "_NET_WM_ICON_NAME",
	"_NET_WM_DESKTOP", "_NET_WM_STATE", "_NET_WM_STATE_MODAL",
	"_NET_WM_STATE_STICKY", "_NET_WM_STATE_MAXIMIZED_VERT",
	"_NET_WM_STATE_MAXIMIZED_HORZ", "_NET_WM_STATE_SHADED",
	"_NET_WM_STATE_SKIP_TASKBAR", "_NET_WM_STATE_SKIP_PAGER",
	"_NET_WM_STATE_HIDDEN", "_NET_WM_STATE_FULLSCREEN", "_NET_WM_STATE_ABOVE",
	"_NET_WM_STATE_BELOW", "_NET_WM_STATE_DEMANDS_ATTENTION",
	"_NET_WM_STATE_FOCUSED", "_NET_WM_ALLOWED_ACTIONS",
	"_NET_WM_ACTION_MOVE", "_NET_WM_ACTION_RESIZE",
	"_NET_WM_ACTION_FULLSCREEN", "_NET_WM_ACTION_MINIMIZE",
	"_NET_WM_ACTION_SHADE", "_NET_WM_ACTION_STICK",
	"_NET_WM_ACTION_MAXIMIZE_HORZ", "_NET_WM_ACTION_MAXIMIZE_VERT",
	"_NET_WM_ACTION_CHANGE_DESKTOP", "_NET_WM_ACTION_CLOSE",
	"_NET_WM_ACTION_ABOVE", "_NET_WM_ACTION_BELOW",
	"_NET_WM_STRUT", "_NET_WM_STRUT_PARTIAL", "_NET_WM_ICON", "_NET_WM_PID",
	"_NET_WM_WINDOW_OPACITY", "_NET_WM_WINDOW_TYPE",
	"_NET_WM_WINDOW_TYPE_DESKTOP", "_NET_WM_WINDOW_TYPE_DOCK",
	"_NET_WM_WINDOW_TYPE_TOOLBAR", "_NET_WM_WINDOW_TYPE_MENU",
	"_NET_WM_WINDOW_TYPE_UTILITY", "_NET_WM_WINDOW_TYPE_SPLASH",
	"_NET_WM_WINDOW_TYPE_DIALOG", "_NET_WM_WINDOW_TYPE_NORMAL",
	"_NET_WM_HANDLED_ICONS", "_NET_RESTACK_WINDOW", "_NET_CLOSE_WINDOW",
	"_NET_MOVERESIZE_WINDOW", "_NET_REQUEST_FRAME_EXTENTS",
	"_NET_FRAME_EXTENTS", "_NET_WM_FULLSCREEN_MONITORS", "_NET_WM_USER_TIME",
};
_Static_assert((int)(sizeof(net_names) / sizeof(net_names[0])) == NET_LAST,
               "net_names/NET_LAST eslesmedi");

void ewmh_init(Display *dpy, Atom *net)
{
	for (int i = 0; i < NET_LAST; i++)
		net[i] = XInternAtom(dpy, net_names[i], False);
}

static const char *const ic_names[] = {
	"WM_PROTOCOLS", "WM_DELETE_WINDOW", "WM_TAKE_FOCUS", "WM_HINTS",
	"WM_NORMAL_HINTS", "WM_CLASS", "WM_NAME", "WM_STATE",
	"_NET_WM_USER_TIME", "WM_CHANGE_STATE",
};
enum {
	IC_PROTOCOLS, IC_DELETE, IC_TAKEFOCUS, IC_HINTS, IC_NORMAL, IC_CLASS,
	IC_NAME, IC_STATE, IC_UTIME, IC_CHANGESTATE,
};

void icccm_init(Display *dpy, Atom *ic)
{
	for (int i = 0; i < (int)(sizeof(ic_names) / sizeof(ic_names[0])); i++)
		ic[i] = XInternAtom(dpy, ic_names[i], False);
}

/* ------------------------------------------------------------------ */
/* Okuyucular                                                          */
/* ------------------------------------------------------------------ */

static char *get_text_prop(Display *dpy, Window w, Atom prop)
{
	Atom type = None;
	int format = 0;
	unsigned long n = 0, after = 0;
	unsigned char *data = NULL;

	if (XGetWindowProperty(dpy, w, prop, 0, 1024, False, AnyPropertyType,
	                       &type, &format, &n, &after, &data) == Success &&
	    data) {
		char *s = xstrndup((const char *)data, n);
		XFree(data);
		return s;
	}
	if (data)
		XFree(data);
	return NULL;
}

char *ewmh_get_name(Display *dpy, Window w)
{
	/* 1) _NET_WM_NAME (UTF8_STRING)  2) WM_NAME (COMPOUND_TEXT) */
	Atom netname = XInternAtom(dpy, "_NET_WM_NAME", False);
	char *s = get_text_prop(dpy, w, netname);
	if (s && *s)
		return s;
	free(s);
	return get_text_prop(dpy, w, XA_WM_NAME);
}

char *ewmh_get_class(Display *dpy, Window w)
{
	XClassHint ch;
	if (XGetClassHint(dpy, w, &ch)) {
		char *r = NULL;
		/* format "instance\tclass" */
		if (ch.res_class) {
			r = xstrdup(ch.res_class);
		} else if (ch.res_name) {
			r = xstrdup(ch.res_name);
		}
		if (ch.res_name) XFree(ch.res_name);
		if (ch.res_class) XFree(ch.res_class);
		return r;
	}
	return NULL;
}

pid_t ewmh_get_pid(Display *dpy, Window w)
{
	Atom pidatom = XInternAtom(dpy, "_NET_WM_PID", False);
	Atom type = None;
	int format = 0;
	unsigned long n = 0, after = 0;
	unsigned char *data = NULL;
	pid_t pid = 0;
	if (XGetWindowProperty(dpy, w, pidatom, 0, 1, False, XA_CARDINAL,
	                       &type, &format, &n, &after, &data) == Success &&
	    data && n >= 1) {
		pid = (pid_t)((long *)data)[0];
	}
	if (data) XFree(data);
	return pid;
}

int ewmh_get_cardinal(Display *dpy, Window root, Atom prop, long *out)
{
	Atom type = None;
	int format = 0;
	unsigned long n = 0, after = 0;
	unsigned char *data = NULL;
	if (XGetWindowProperty(dpy, root, prop, 0, 1, False, XA_CARDINAL,
	                       &type, &format, &n, &after, &data) == Success &&
	    data && n >= 1) {
		*out = (long)((long *)data)[0];
		if (data) XFree(data);
		return 1;
	}
	if (data) XFree(data);
	return 0;
}

int ewmh_get_client_list(Display *dpy, Window root, Atom prop, Window **out)
{
	Atom type = None;
	int format = 0;
	unsigned long n = 0, after = 0;
	unsigned char *data = NULL;
	*out = NULL;
	if (XGetWindowProperty(dpy, root, prop, 0, 16384, False, XA_WINDOW,
	                       &type, &format, &n, &after, &data) == Success &&
	    data && type == XA_WINDOW && n > 0) {
		Window *list = xmalloc(n * sizeof(Window));
		memcpy(list, data, n * sizeof(Window));
		XFree(data);
		*out = list;
		return (int)n;
	}
	if (data) XFree(data);
	return 0;
}

/* _NET_WM_STATE içinde fullscreen var mı? */
static int state_has(Display *dpy, Window w, Atom *net, Atom want)
{
	Atom type = None;
	int format = 0;
	unsigned long n = 0, after = 0;
	unsigned char *data = NULL;
	int found = 0;
	if (XGetWindowProperty(dpy, w, net[NET_WM_STATE], 0, 64, False,
	                       XA_ATOM, &type, &format, &n, &after, &data) ==
	        Success &&
	    data && type == XA_ATOM) {
		Atom *a = (Atom *)data;
		for (unsigned long i = 0; i < n; i++)
			if (a[i] == want)
				found = 1;
	}
	if (data) XFree(data);
	return found;
}

int ewmh_get_state_fullscreen(Display *dpy, Atom *net, Window w)
{
	return state_has(dpy, w, net, net[NET_WM_STATE_FULLSCREEN]);
}

/* _NET_WM_WINDOW_TYPE'ı okur; varsayılan NORMAL. */
static Atom ewmh_get_type_atom(Display *dpy, Atom *net, Window w)
{
	Atom type = None;
	int format = 0;
	unsigned long n = 0, after = 0;
	unsigned char *data = NULL;
	Atom a = net[NET_WM_WINDOW_TYPE_NORMAL];
	if (XGetWindowProperty(dpy, w, net[NET_WM_WINDOW_TYPE], 0, 1, False,
	                       XA_ATOM, &type, &format, &n, &after, &data) ==
	        Success &&
	    data && type == XA_ATOM && n >= 1)
		a = ((Atom *)data)[0];
	if (data) XFree(data);
	return a;
}

int ewmh_is_normal_window(Display *dpy, Atom *net, Window w)
{
	Atom t = ewmh_get_type_atom(dpy, net, w);
	if (t == net[NET_WM_WINDOW_TYPE_DESKTOP] ||
	    t == net[NET_WM_WINDOW_TYPE_DOCK] ||
	    t == net[NET_WM_WINDOW_TYPE_TOOLBAR] ||
	    t == net[NET_WM_WINDOW_TYPE_MENU] ||
	    t == net[NET_WM_WINDOW_TYPE_SPLASH] ||
	    t == net[NET_WM_WINDOW_TYPE_UTILITY])
		return 0;
	return 1;
}

/* ------------------------------------------------------------------ */
/* Yazıcılar                                                           */
/* ------------------------------------------------------------------ */

static void change_prop32(Display *dpy, Window w, Atom prop, int mode,
                          const long *data, int n)
{
	XChangeProperty(dpy, w, prop, XA_CARDINAL, 32, mode,
	                (const unsigned char *)data, n);
}

void ewmh_set_num_desktops(Display *dpy, Window root, Atom prop, int n)
{
	long v = n;
	change_prop32(dpy, root, prop, PropModeReplace, &v, 1);
}

void ewmh_set_current_desktop(Display *dpy, Window root, Atom prop, int idx)
{
	long v = idx;
	change_prop32(dpy, root, prop, PropModeReplace, &v, 1);
}

void ewmh_set_active_window(Display *dpy, Window root, Atom prop, Window w)
{
	long v = (long)w;
	change_prop32(dpy, root, prop, PropModeReplace, &v, 1);
}

void ewmh_set_wm_desktop(Display *dpy, Window w, Atom prop, long idx)
{
	change_prop32(dpy, w, prop, PropModeReplace, &idx, 1);
}

void ewmh_set_client_list(Display *dpy, Window root, Atom prop, Window *list,
                          int n)
{
	XChangeProperty(dpy, root, prop, XA_WINDOW, 32, PropModeReplace,
	                (const unsigned char *)list, n);
}

void ewmh_set_desktop_names(Display *dpy, Window root, Atom prop,
                            const char **names, int n)
{
	size_t total = 0;
	for (int i = 0; i < n; i++)
		total += strlen(names[i]) + 1;
	char *buf = xcalloc(total ? total : 1, 1);
	char *p = buf;
	for (int i = 0; i < n; i++) {
		size_t l = strlen(names[i]);
		memcpy(p, names[i], l);
		p += l;
		*p++ = '\0';
	}
	XChangeProperty(dpy, root, prop,
	                XInternAtom(dpy, "UTF8_STRING", False), 8,
	                PropModeReplace, (const unsigned char *)buf,
	                (int)total);
	free(buf);
}

void ewmh_set_workarea(Display *dpy, Window root, Atom prop, int x, int y,
                       int w, int h, int nd)
{
	long data[16];
	for (int i = 0; i < nd && i < 4; i++) {
		data[i * 4 + 0] = x;
		data[i * 4 + 1] = y;
		data[i * 4 + 2] = w;
		data[i * 4 + 3] = h;
	}
	int n = nd < 4 ? nd : 4;
	XChangeProperty(dpy, root, prop, XA_CARDINAL, 32, PropModeReplace,
	                (const unsigned char *)data, n * 4);
}

void ewmh_set_wm_check(Display *dpy, Window root, Window check)
{
	long v = (long)check;
	change_prop32(dpy, root, XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False),
	              PropModeReplace, &v, 1);
}

void ewmh_set_supported(Display *dpy, Window root, Atom *net)
{
	XChangeProperty(dpy, root, net[NET_SUPPORTED], XA_ATOM, 32,
	                PropModeReplace, (const unsigned char *)net, NET_LAST);
}

void ewmh_update_state(Display *dpy, Window w, Atom prop, Atom action,
                       Atom *state, int nstate)
{
	XEvent e;
	memset(&e, 0, sizeof(e));
	e.xclient.type = ClientMessage;
	e.xclient.window = w;
	e.xclient.message_type = prop;
	e.xclient.format = 32;
	e.xclient.data.l[0] = (long)action;
	e.xclient.data.l[1] = nstate > 0 ? (long)state[0] : 0;
	e.xclient.data.l[2] = nstate > 1 ? (long)state[1] : 0;
	e.xclient.data.l[3] = 1;
	XSendEvent(dpy, DefaultRootWindow(dpy), False,
	           SubstructureRedirectMask | SubstructureNotifyMask, &e);
	XFlush(dpy);
}

void ewmh_set_wm_state_hidden(Display *dpy, Window w, Atom *net, int hidden)
{
	ewmh_update_state(dpy, w, net[NET_WM_STATE],
	                  hidden ? (Atom)1 : (Atom)0 /* _NET_WM_STATE_ADD / REMOVE */,
	                  &net[NET_WM_STATE_HIDDEN], 1);
}

void ewmh_set_wm_state_fullscreen(Display *dpy, Window w, Atom *net, int on)
{
	ewmh_update_state(dpy, w, net[NET_WM_STATE],
	                  on ? (Atom)1 : (Atom)0,
	                  &net[NET_WM_STATE_FULLSCREEN], 1);
}

/* _NET_WM_STATE özelliğini okuyup istenen bayrakları ekleyerek/çıkararak
 * doğrudan yazar (uygulamalar ve panel bu özelliği okur). */
void ewmh_set_state_flags(Display *dpy, Window w, Atom state_prop,
                          Atom fullscreen_atom, Atom hidden_atom,
                          int fullscreen, int hidden)
{
	Atom type = None;
	int format = 0;
	unsigned long n = 0, after = 0;
	unsigned char *data = NULL;
	Atom *cur = NULL;
	int ncur = 0;

	if (XGetWindowProperty(dpy, w, state_prop, 0, 128, False, XA_ATOM,
	                       &type, &format, &n, &after, &data) == Success &&
	    data && type == XA_ATOM)
		cur = (Atom *)data, ncur = (int)n;

	Atom *out = xmalloc((size_t)(ncur + 2) * sizeof(Atom));
	int no = 0;

	for (int i = 0; i < ncur; i++) {
		if (fullscreen >= 0 && cur[i] == fullscreen_atom)
			continue; /* yeniden eklenecek */
		if (hidden >= 0 && cur[i] == hidden_atom)
			continue;
		out[no++] = cur[i];
	}
	if (fullscreen == 1 && fullscreen_atom)
		out[no++] = fullscreen_atom;
	if (hidden == 1 && hidden_atom)
		out[no++] = hidden_atom;

	if (data)
		XFree(data);
	XChangeProperty(dpy, w, state_prop, XA_ATOM, 32, PropModeReplace,
	                (const unsigned char *)out, no);
	free(out);
}

/* ------------------------------------------------------------------ */
/* ICCCM                                                               */
/* ------------------------------------------------------------------ */

static int has_wm_protocol(Display *dpy, Window w, Atom wm_protocols,
                           Atom proto)
{
	Atom type = None;
	int format = 0;
	unsigned long n = 0, after = 0;
	unsigned char *data = NULL;
	int found = 0;
	if (XGetWindowProperty(dpy, w, wm_protocols, 0, 64, False, XA_ATOM,
	                       &type, &format, &n, &after, &data) == Success &&
	    data && type == XA_ATOM) {
		Atom *a = (Atom *)data;
		for (unsigned long i = 0; i < n; i++)
			if (a[i] == proto)
				found = 1;
	}
	if (data) XFree(data);
	return found;
}

int icccm_has_proto(Display *dpy, Window w, Atom wm_protocols, Atom wm_delete)
{
	return has_wm_protocol(dpy, w, wm_protocols, wm_delete);
}

void icccm_take_focus(Display *dpy, Window w, Atom wm_protocols,
                      Atom wm_take_focus, Window focus_ts)
{
	(void)focus_ts;
	if (!has_wm_protocol(dpy, w, wm_protocols, wm_take_focus))
		return;
	XEvent e;
	memset(&e, 0, sizeof(e));
	e.xclient.type = ClientMessage;
	e.xclient.window = w;
	e.xclient.message_type = wm_protocols;
	e.xclient.format = 32;
	e.xclient.data.l[0] = (long)wm_take_focus;
	e.xclient.data.l[1] = (long)CurrentTime;
	XSendEvent(dpy, w, False, NoEventMask, &e);
	XFlush(dpy);
}

void icccm_wm_change_state(Display *dpy, Window w, Atom wm_protocols,
                           Atom wm_change_state, int iconic)
{
	if (!has_wm_protocol(dpy, w, wm_protocols, wm_change_state))
		return;
	XEvent e;
	memset(&e, 0, sizeof(e));
	e.xclient.type = ClientMessage;
	e.xclient.window = w;
	e.xclient.message_type = wm_protocols;
	e.xclient.format = 32;
	e.xclient.data.l[0] = (long)wm_change_state;
	e.xclient.data.l[1] = (long)IconicState;
	XSendEvent(dpy, w, False, NoEventMask, &e);
	XFlush(dpy);
	(void)iconic;
}
