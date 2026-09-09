/* linis_ewmh.h — EWMH net atomları ve yardımcı işlevler */
#ifndef LINIS_EWMH_H
#define LINIS_EWMH_H

#include <X11/Xlib.h>
#include <X11/Xatom.h>

enum {
	NET_SUPPORTED, NET_SUPPORTING_WM_CHECK, NET_CLIENT_LIST,
	NET_CLIENT_LIST_STACKING, NET_NUMBER_OF_DESKTOPS, NET_DESKTOP_GEOMETRY,
	NET_DESKTOP_VIEWPORT, NET_CURRENT_DESKTOP, NET_DESKTOP_NAMES,
	NET_ACTIVE_WINDOW, NET_WORKAREA, NET_WM_NAME, NET_WM_VISIBLE_NAME,
	NET_WM_ICON_NAME, NET_WM_DESKTOP, NET_WM_STATE, NET_WM_STATE_MODAL,
	NET_WM_STATE_STICKY, NET_WM_STATE_MAXIMIZED_VERT,
	NET_WM_STATE_MAXIMIZED_HORZ, NET_WM_STATE_SHADED, NET_WM_STATE_SKIP_TASKBAR,
	NET_WM_STATE_SKIP_PAGER, NET_WM_STATE_HIDDEN, NET_WM_STATE_FULLSCREEN,
	NET_WM_STATE_ABOVE, NET_WM_STATE_BELOW, NET_WM_STATE_DEMANDS_ATTENTION,
	NET_WM_STATE_FOCUSED, NET_WM_ALLOWED_ACTIONS, NET_WM_ACTION_MOVE,
	NET_WM_ACTION_RESIZE, NET_WM_ACTION_FULLSCREEN, NET_WM_ACTION_MINIMIZE,
	NET_WM_ACTION_SHADE, NET_WM_ACTION_STICK, NET_WM_ACTION_MAXIMIZE_HORZ,
	NET_WM_ACTION_MAXIMIZE_VERT, NET_WM_ACTION_CHANGE_DESKTOP,
	NET_WM_ACTION_CLOSE, NET_WM_ACTION_ABOVE, NET_WM_ACTION_BELOW,
	NET_WM_STRUT, NET_WM_STRUT_PARTIAL, NET_WM_ICON, NET_WM_PID,
	NET_WM_WINDOW_OPACITY, NET_WM_WINDOW_TYPE,
	NET_WM_WINDOW_TYPE_DESKTOP, NET_WM_WINDOW_TYPE_DOCK,
	NET_WM_WINDOW_TYPE_TOOLBAR, NET_WM_WINDOW_TYPE_MENU,
	NET_WM_WINDOW_TYPE_UTILITY, NET_WM_WINDOW_TYPE_SPLASH,
	NET_WM_WINDOW_TYPE_DIALOG, NET_WM_WINDOW_TYPE_NORMAL,
	NET_WM_HANDLED_ICONS, NET_RESTACK_WINDOW, NET_CLOSE_WINDOW,
	NET_MOVERESIZE_WINDOW, NET_REQUEST_FRAME_EXTENTS, NET_FRAME_EXTENTS,
	NET_WM_FULLSCREEN_MONITORS, NET_WM_USER_TIME,
	NET_LAST, /* sentinel */
};

/* Tüm net atomlarını al (tek seferde, NET_LAST boyutunda dizi). */
void ewmh_init(Display *dpy, Atom *net);

/* ICCCM / eski uyum */
void icccm_init(Display *dpy, Atom *ic);

/* Pencere başlığı: _NET_WM_NAME (UTF8) veya WM_NAME. */
char *ewmh_get_name(Display *dpy, Window w);
/* _NET_WM_CLASS → "instance" veya "class". malloc. */
char *ewmh_get_class(Display *dpy, Window w);
/* _NET_WM_PID → pid; yoksa 0. */
pid_t ewmh_get_pid(Display *dpy, Window w);

/* _NET_WM_WINDOW_TYPE: normal/önemsiz kararı.
 * Desktop, dock, toolbar, menu, splash → "utility" say. */
int  ewmh_is_normal_window(Display *dpy, Atom *net, Window w);

int  ewmh_get_state_fullscreen(Display *dpy, Atom *net, Window w);

/* --- pencere listesi (panel/launcher okur) --- */
int  ewmh_get_client_list(Display *dpy, Window root, Atom prop, Window **out);
int  ewmh_get_cardinal(Display *dpy, Window root, Atom prop, long *out);

/* --- setter'lar (WM tarafı) --- */
void ewmh_set_supported(Display *dpy, Window root, Atom *net);
void ewmh_set_wm_check(Display *dpy, Window root, Window check);
void ewmh_set_num_desktops(Display *dpy, Window root, Atom net_num, int n);
void ewmh_set_current_desktop(Display *dpy, Window root, Atom prop, int idx);
void ewmh_set_desktop_names(Display *dpy, Window root, Atom prop,
                            const char **names, int n);
void ewmh_set_active_window(Display *dpy, Window root, Atom prop, Window w);
void ewmh_set_client_list(Display *dpy, Window root, Atom prop, Window *list,
                          int n);
void ewmh_set_workarea(Display *dpy, Window root, Atom prop, int x, int y,
                       int w, int h, int nd);
void ewmh_set_wm_desktop(Display *dpy, Window w, Atom prop, long idx);
void ewmh_set_wm_state_hidden(Display *dpy, Window w, Atom *net, int hidden);
void ewmh_set_wm_state_fullscreen(Display *dpy, Window w, Atom *net, int on);

/* Pencere _NET_WM_STATE özelliğini gerçekten güncelle (ClientMessage değil).
 * fullscreen/hidden -1 ise dokunulmaz, 0 kaldır, 1 ekle. */
void ewmh_set_state_flags(Display *dpy, Window w, Atom state_prop,
                          Atom fullscreen_atom, Atom hidden_atom,
                          int fullscreen, int hidden);

/* Atom dizisinde _NET_WM_STATE üyeliği olan pencereye state'i ekle/çıkar. */
void ewmh_update_state(Display *dpy, Window w, Atom prop, Atom action,
                       Atom *state, int nstate);

/* Pencereye odak iste (WM_TAKE_FOCUS varsa onu kullan). */
void icccm_take_focus(Display *dpy, Window w, Atom wm_protocols,
                      Atom wm_take_focus, Window focus_ts);
/* WM_DELETE_WINDOW protokolü var mı? */
int  icccm_has_proto(Display *dpy, Window w, Atom wm_protocols,
                     Atom wm_delete);
/* WM_CHANGE_STATE ile minimize iste. */
void icccm_wm_change_state(Display *dpy, Window w, Atom wm_protocols,
                           Atom wm_change_state, int iconic);

#endif
