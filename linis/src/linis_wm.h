/* linis_wm.h — çekirdek WM uygulamasının dışa açık API'si */
#ifndef LINIS_WM_H
#define LINIS_WM_H

#include <X11/Xlib.h>

#include "linis_config.h"

typedef struct LinisWM LinisWM;

LinisWM *wm_create(Display *dpy, const LConfig *cfg);
void wm_destroy(LinisWM *wm);

/* X olaylarını WM'ye yönlendir. 1 = işlendi. */
int wm_handle_event(LinisWM *wm, const XEvent *ev);

/* Her döngü turunda: kısayol tick + kare. 1 = kare basıldı. */
int wm_loop_tick(LinisWM *wm);

/* Bir sonraki beklemeye kadar ms (select/poll zaman aşımı). */
int wm_timeout_ms(LinisWM *wm);

/* WM'i yeniden başlat (oturumu koru). */
void wm_restart(LinisWM *wm);
void wm_quit(LinisWM *wm);
void wm_reload_wallpaper(LinisWM *wm);

/* Döngüde: WM çıkmak / yeniden başlamak istiyor mu? */
int wm_wants_quit(LinisWM *wm);
int wm_wants_restart(LinisWM *wm);

#endif
