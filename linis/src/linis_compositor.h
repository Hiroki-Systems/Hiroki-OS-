/*
 * linis_compositor.h — built-in XRender compositor.
 * Gölge, yuvarlak köşe, blur, aktif/pasif opaklık ve animasyonlar
 * burada yaşar (LINIS_DESIGN.md §4).
 */
#ifndef LINIS_COMPOSITOR_H
#define LINIS_COMPOSITOR_H

#include <X11/Xlib.h>

#include "linis_config.h"

typedef struct LCompositor LCompositor;

/* Compositor'ü başlat: XComposite alt pencere yönlendirmesi açılır,
 * backbuffer kurulur, ilk kare çizilir. */
LCompositor *comp_create(Display *dpy, int scr, const LConfig *cfg);
void comp_destroy(LCompositor *c);

/* X olaylarını besle (DamageNotify vb.). 1 = bir şey çizilecek. */
int comp_handle_event(LCompositor *c, const XEvent *e);

/* Pencere yönetimine alındı/görünür oldu → takip + hasar izle. */
void comp_win_map(LCompositor *c, Window w);
/* Pencere kaldırıldı/yok oldu. */
void comp_win_unmap(LCompositor *c, Window w);

/* Odak değişince (pasif pencere opaklığı için). */
void comp_set_active(LCompositor *c, Window w);

/* Her X olay turunun sonunda çağrılır; gerekirse kare basar.
 * 1 = kare basıldı. */
int  comp_frame(LCompositor *c);

/* Bir sonraki yeniden çizime kadar beklenebilecek ms. (select/poll) */
int  comp_timeout_ms(LCompositor *c);

/* Animasyonlar */
void comp_anim_open(LCompositor *c, Window w);   /* açılırken scale-up */
void comp_anim_minimize(LCompositor *c, Window w);/* minimize scale-down;
                                                  * bitince pencere unmaps */
void comp_anim_slide(LCompositor *c, int dir);   /* workspace slide: ±1 */

/* Duvar kağıdını root'tan yeniden yakala (SIGUSR1 / wallpaper değişimi). */
void comp_wall_recapture(LCompositor *c);

#endif
