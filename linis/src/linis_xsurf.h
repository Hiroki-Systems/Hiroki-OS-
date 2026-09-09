/*
 * linis_xsurf.h — XRender yüzey (pixmap+Picture) ve efekt yardımcıları.
 * Compositor'ün alt katmanı. Tüm iç yüzeyler ARGB32'dir; "maske"
 * yüzeylerinde alfa kanalı kapsama (coverage) taşır.
 */
#ifndef LINIS_XSURF_H
#define LINIS_XSURF_H

#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

typedef struct {
	Pixmap pm;
	Picture pic;
	int    w, h;
	XRenderPictFormat *fmt;
} XSurf;

/* ARGB32 boş (şeffaf) yüzey. */
XSurf xsurf_new_argb(Display *dpy, int scr, int w, int h);
void  xsurf_free(Display *dpy, XSurf *s);
void  xsurf_clear(Display *dpy, XSurf *s);

/* Alfa kanalını 0..255 kapsamaya göre CPU'da doldurabilmek için tüm
 * byte'lar eşit yazılır; XPutImage ardından hangi byte'ın alfa olduğu
 * önemsizleşir (Render formatı üst byte'ı alfa sayar). */

void xr_copy(Display *dpy, Picture src, Picture dst,
             int sx, int sy, int dx, int dy, int w, int h);
void xr_fill(Display *dpy, Picture dst, int x, int y, int w, int h,
             double alpha);

/* src yüzeyinin [sx,sy,w,h] bölgesini kutu-blur ile yumuşatıp yeni bir
 * yüzey döner. radius <= 1 kopyaya dönüşür. */
XSurf xsurf_box_blur(Display *dpy, int scr, XSurf src,
                     int sx, int sy, int w, int h, int radius);

/* Bir pixmap + visual için content picture üretir (window content). */
Picture xr_picture_from_pixmap(Display *dpy, Pixmap pm, int depth,
                               Visual *vis);

/* Kenar yumuşatmalı yuvarlak köşe maskesi (alfa = kapsama). */
XSurf xsurf_rounded_mask(Display *dpy, int scr, int w, int h, int r);

/* Pencere maskesi: köşe kapsaması × global alfa. r<=0 ise düz dikdörtgen
 * alfa. alpha 1'e yakınken de aynı yolu kullanmak güvenli. */
XSurf xsurf_win_mask(Display *dpy, int scr, int w, int h, int r, double alpha);

/* Merkezden büyüyen dikdörtgen "reveal" maskesi; t ∈ [0,1]. */
XSurf xsurf_reveal_mask(Display *dpy, int scr, int w, int h, double t);

/* Yumuşak gölge: pencere w×h için pad eklenmiş siyah gölge yüzeyi.
 * width = w + 2*pad, pad = radius. Gölge aşağı offset ile sürülür. */
XSurf xsurf_shadow(Display *dpy, int scr, int w, int h, int radius,
                   double opacity);

#endif
