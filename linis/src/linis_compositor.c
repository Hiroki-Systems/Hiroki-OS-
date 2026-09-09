/*
 * linis_compositor.c — built-in XRender compositor (LINIS_DESIGN.md §4).
 *
 * Model:
 *  - XCompositeRedirectSubwindows(root, Automatic): tüm müşteri pencereleri
 *    ekran dışına yönlendirilir.
 *  - Her yeniden çizim TAM karedir: opak arka plan yüzeyi (wallpaper
 *    kopyası) → alttan üste tüm görünür pencereler (gölge/köşe/opaklık/
 *    blur) → backbuffer root'a PictOpSrc ile basılır.
 *  - Yeniden çizim yalnızca damage olayı ya da animasyon karesi varken
 *    yapılır; idle'da CPU %0.
 *
 * Not: "blur" ve "gölge" XRender yüzey kopyaları + kutusal yumuşatma ile
 * çizilir; GL kullanılmaz (tasarım gereği GLX yok).
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xdamage.h>

#include "linis_compositor.h"
#include "linis_xsurf.h"
#include "linis_ewmh.h"
#include "linis_anim.h"
#include "linis_util.h"

typedef struct LCompW {
	Window   w;
	Damage   dmg;
	int      x, y, wid, ht;      /* son bilinen geometri */
	int      mapped;
	int      ovr;                /* override_redirect */
	int      dock;               /* dock/toolbar/menu/utility tipi */
	unsigned long clopacity;     /* _NET_WM_WINDOW_OPACITY, 0xffffffff=yok */
	int      blur;
	int      blur_r;             /* _LINIS_BLUR_RADIUS (0 = cfg varsayılan) */

	/* önbellekler */
	XSurf    mask;
	int      mask_key;           /* w/h/r/alfa özeti */
	XSurf    shadow;
	int      shadow_key;

	Tween    rev;                /* reveal 0..1 */
	int      wants_close;        /* minimize sonrası unmap */

	int      freed;
	struct LCompW *next;
} LCompW;

struct LCompositor {
	Display *dpy;
	int      scr;
	Window   root;
	int      sw, sh;
	const LConfig *cfg;

	XSurf    back;               /* fullscreen ARGB birleştirme tamponu */
	XSurf    wall;               /* wallpaper katmanı (başlangıç kopyası) */
	Picture  rootpic;            /* root üzerindeki hedef picture */

	LCompW  *wins;               /* damage + efekt takibi */
	int      dirty;
	int      animating;

	/* workspace slide */
	int      slide_on;
	int      slide_dir;
	XSurf    snap;
	Tween    slide_t;

	/* atomlar */
	Atom net[NET_LAST];
	Atom at_opacity, at_blur, at_active, at_type;
	Window active;

	int dmg_ev_base;            /* XDamage olay tabanı */
};

/* ------------------------------------------------------------------ */
/* Yardımcılar                                                         */
/* ------------------------------------------------------------------ */

static LCompW *find_win(LCompositor *c, Window w)
{
	for (LCompW *e = c->wins; e; e = e->next)
		if (e->w == w && !e->freed)
			return e;
	return NULL;
}

static void mark_full(LCompositor *c)
{
	c->dirty = 1;
}

static int rect_intersect(int x, int y, int w, int h,
                          int x2, int y2, int w2, int h2)
{
	return x < x2 + w2 && x2 < x + w && y < y2 + h2 && y2 < y + h;
}

/* EWMH tipine göre "özel" (gölge/köşe/animasyon yok) mu? */
static int is_special_type(Display *dpy, Atom *net, Window w)
{
	Atom type = None;
	int format = 0;
	unsigned long n = 0, after = 0;
	unsigned char *data = NULL;
	Atom a = 0;
	if (XGetWindowProperty(dpy, w, net[NET_WM_WINDOW_TYPE], 0, 1, False,
	                       XA_ATOM, &type, &format, &n, &after, &data) ==
	        Success &&
	    data && type == XA_ATOM && n >= 1)
		a = ((Atom *)data)[0];
	if (data) XFree(data);
	return a == net[NET_WM_WINDOW_TYPE_DESKTOP] ||
	       a == net[NET_WM_WINDOW_TYPE_DOCK] ||
	       a == net[NET_WM_WINDOW_TYPE_TOOLBAR] ||
	       a == net[NET_WM_WINDOW_TYPE_MENU] ||
	       a == net[NET_WM_WINDOW_TYPE_UTILITY] ||
	       a == net[NET_WM_WINDOW_TYPE_SPLASH];
}

static unsigned long read_opacity(Display *dpy, Window w)
{
	Atom atom = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
	Atom type = None;
	int format = 0;
	unsigned long n = 0, after = 0;
	unsigned char *data = NULL;
	unsigned long v = 0xffffffffUL;
	if (XGetWindowProperty(dpy, w, atom, 0, 1, False, XA_CARDINAL, &type,
	                       &format, &n, &after, &data) == Success && data &&
	    n >= 1)
		v = (unsigned long)((long *)data)[0];
	if (data) XFree(data);
	return v;
}

static int read_blur(Display *dpy, Window w)
{
	Atom atom = XInternAtom(dpy, "_LINIS_BLUR_RADIUS", False);
	Atom type = None;
	int format = 0;
	unsigned long n = 0, after = 0;
	unsigned char *data = NULL;
	int v = 0;
	if (XGetWindowProperty(dpy, w, atom, 0, 1, False, XA_CARDINAL, &type,
	                       &format, &n, &after, &data) == Success && data &&
	    n >= 1)
		v = (int)((long *)data)[0];
	if (data) XFree(data);
	return v;
}

/* ------------------------------------------------------------------ */
/* Kayıt (damage takibi)                                               */
/* ------------------------------------------------------------------ */

static void win_query_geom(LCompositor *c, LCompW *e)
{
	XWindowAttributes a;
	if (XGetWindowAttributes(c->dpy, e->w, &a)) {
		e->x = a.x;
		e->y = a.y;
		e->wid = a.width;
		e->ht = a.height;
		e->mapped = a.map_state == IsViewable;
		e->ovr = a.override_redirect != 0;
		if (a.class == InputOnly)
			e->wid = e->ht = 0;
	}
}

static LCompW *win_add(LCompositor *c, Window w)
{
	LCompW *e = xcalloc(1, sizeof(*e));
	e->w = w;
	e->clopacity = 0xffffffffUL;
	e->blur = 0;
	e->mask.pm = None;
	e->shadow.pm = None;
	e->mapped = 1;
	e->freed = 0;
	win_query_geom(c, e);
	e->dock = is_special_type(c->dpy, c->net, w);
	e->clopacity = read_opacity(c->dpy, w);
	e->blur_r = read_blur(c->dpy, w);
	if (e->blur_r > 0)
		e->blur = 1;
	e->dmg = XDamageCreate(c->dpy, w, XDamageReportNonEmpty);
	e->next = c->wins;
	c->wins = e;
	return e;
}

static void win_remove(LCompositor *c, LCompW *e)
{
	if (e->dmg)
		XDamageDestroy(c->dpy, e->dmg);
	if (e->mask.pm)
		xsurf_free(c->dpy, &e->mask);
	if (e->shadow.pm)
		xsurf_free(c->dpy, &e->shadow);
	e->freed = 1;
}

void comp_win_map(LCompositor *c, Window w)
{
	LCompW *e = find_win(c, w);
	if (!e)
		e = win_add(c, w);
	e->freed = 0;
	win_query_geom(c, e);
	e->dock = is_special_type(c->dpy, c->net, w);
	e->clopacity = read_opacity(c->dpy, w);
	e->blur_r = read_blur(c->dpy, w);
	e->blur = e->blur_r > 0;
	e->wants_close = 0;
	/* Açılma animasyonu comp_anim_open ile ayrıca tetiklenir. */
	mark_full(c);
}

void comp_win_unmap(LCompositor *c, Window w)
{
	LCompW *e = find_win(c, w);
	if (!e)
		return;
	/* yok olan/ikonlaşan pencerenin yeri yeniden çizilmeli */
	mark_full(c);
	win_remove(c, e);
}

void comp_set_active(LCompositor *c, Window w)
{
	c->active = w;
	mark_full(c);
}

/* ------------------------------------------------------------------ */
/* Yaratım / yıkım                                                     */
/* ------------------------------------------------------------------ */

LCompositor *comp_create(Display *dpy, int scr, const LConfig *cfg)
{
	LCompositor *c = xcalloc(1, sizeof(*c));
	c->dpy = dpy;
	c->scr = scr;
	c->root = RootWindow(dpy, scr);
	c->cfg = cfg;

	int screenno = DefaultScreen(dpy);
	Screen *s = ScreenOfDisplay(dpy, screenno);
	c->sw = WidthOfScreen(s);
	c->sh = HeightOfScreen(s);

	/* XDamage / XComposite / XRender mevcut mu? */
	int evb = 0, erb = 0;
	if (!XDamageQueryExtension(dpy, &evb, &erb) ||
	    !XCompositeQueryExtension(dpy, &evb, &erb) ||
	    !XRenderQueryExtension(dpy, &evb, &erb))
		die("compositor: XDamage/XComposite/XRender gerekli");
	c->dmg_ev_base = evb;

	/* root picture (hedef) */
	Visual *vis = DefaultVisual(dpy, scr);
	XRenderPictFormat *rf = XRenderFindVisualFormat(dpy, vis);
	c->rootpic = XRenderCreatePicture(dpy, c->root, rf, 0, NULL);

	/* tam ekran ARGB birleştirme tamponu */
	c->back = xsurf_new_argb(dpy, scr, c->sw, c->sh);

	/* wallpaper katmanı: root'un o anki içeriği (duvar kağıdı önce
	 * ayarlanmalı; aksi halde düz renk). */
	c->wall = xsurf_new_argb(dpy, scr, c->sw, c->sh);
	XRenderComposite(dpy, PictOpOver, c->rootpic, None, c->wall.pic, 0, 0, 0,
	                 0, 0, 0, (unsigned)c->sw, (unsigned)c->sh);

	ewmh_init(dpy, c->net);
	c->at_opacity = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
	c->at_blur = XInternAtom(dpy, "_LINIS_BLUR_RADIUS", False);
	c->at_active = c->net[NET_ACTIVE_WINDOW];
	XSync(dpy, False);

	/* alt pencereleri ekran dışına yönlendir */
	XCompositeRedirectSubwindows(dpy, c->root, CompositeRedirectAutomatic);

	mark_full(c);
	return c;
}

void comp_destroy(LCompositor *c)
{
	XCompositeUnredirectSubwindows(c->dpy, c->root, CompositeRedirectAutomatic);
	LCompW *e = c->wins;
	while (e) {
		LCompW *n = e->next;
		win_remove(c, e);
		free(e);
		e = n;
	}
	if (c->snap.pm)
		xsurf_free(c->dpy, &c->snap);
	xsurf_free(c->dpy, &c->wall);
	xsurf_free(c->dpy, &c->back);
	XRenderFreePicture(c->dpy, c->rootpic);
	free(c);
}

int comp_handle_event(LCompositor *c, const XEvent *e)
{
	/* DamageNotify: bir pencerenin içeriği değişti → tam kare repaint */
	if (e->type == c->dmg_ev_base) {
		mark_full(c);
		return 1;
	}
	return 0;
}

/* Damage sayaçlarını sıfırla (yeniden tetiklenme için). Kare öncesi. */
static void comp_subtract_all(LCompositor *c)
{
	for (LCompW *e = c->wins; e; e = e->next) {
		if (e->dmg && !e->freed)
			XDamageSubtract(c->dpy, e->dmg, None, None);
	}
}

/* ------------------------------------------------------------------ */
/* Animasyonlar                                                        */
/* ------------------------------------------------------------------ */

void comp_anim_open(LCompositor *c, Window w)
{
	LCompW *e = find_win(c, w);
	if (!e || e->dock || e->ovr || !c->cfg->comp.animations)
		return;
	tween_start(&e->rev, EASE_OUT_CUBIC, 0.0, 1.0,
	            c->cfg->comp.animation_ms);
	mark_full(c);
}

void comp_anim_minimize(LCompositor *c, Window w)
{
	LCompW *e = find_win(c, w);
	if (!e || e->dock || e->ovr || !c->cfg->comp.animations) {
		/* animasyon yoksa anında kaldır */
		XUnmapWindow(c->dpy, w);
		return;
	}
	e->wants_close = 1;
	tween_start(&e->rev, EASE_OUT_CUBIC, 1.0, 0.0,
	            c->cfg->comp.animation_ms);
	mark_full(c);
}

void comp_anim_slide(LCompositor *c, int dir)
{
	if (!c->cfg->comp.animations) {
		mark_full(c);
		return;
	}
	/* mevcut kareyi "eski sahne" olarak sakla */
	if (!c->slide_on) {
		if (c->snap.pm)
			xsurf_free(c->dpy, &c->snap);
		c->snap = xsurf_new_argb(c->dpy, c->scr, c->sw, c->sh);
		XRenderComposite(c->dpy, PictOpSrc, c->back.pic, None, c->snap.pic,
		                 0, 0, 0, 0, 0, 0, (unsigned)c->sw, (unsigned)c->sh);
	}
	c->slide_on = 1;
	c->slide_dir = dir;
	tween_start(&c->slide_t, EASE_IN_OUT_CUBIC, 0.0, 1.0,
	            LMAX(150, (int)(c->cfg->comp.animation_ms * 0.83)));
	mark_full(c);
}

/* ------------------------------------------------------------------ */
/* Boyama                                                              */
/* ------------------------------------------------------------------ */

/* Bir düğümün (ve alt ağacının) içeriğini back'e maskeyle basar.
 * top: pencerenin üst düzeyi (maske bununla hizalanır). */
static void paint_subtree(LCompositor *c, Window node, int offx, int offy,
                          LCompW *top, Picture mask, int mx, int my)
{
	Display *dpy = c->dpy;
	XWindowAttributes a;
	if (!XGetWindowAttributes(dpy, node, &a))
		return;
	if (a.map_state != IsViewable || a.width <= 0 || a.height <= 0)
		return;
	if (a.class == InputOnly)
		goto children;

	{
		Pixmap pm = XCompositeNameWindowPixmap(dpy, node);
		if (pm) {
			Picture pic = xr_picture_from_pixmap(dpy, pm, a.depth,
			                                     a.visual);
			if (pic) {
				int ax = offx + a.x;
				int ay = offy + a.y;
				if (mask) {
					XRenderComposite(dpy, PictOpOver, pic, mask,
					                 c->back.pic,
					                 0, 0, /* source 0,0 */
					                 ax - mx, ay - my, /* mask kaydırma */
					                 ax, ay, (unsigned)a.width,
					                 (unsigned)a.height);
				} else {
					XRenderComposite(dpy, PictOpOver, pic, None,
					                 c->back.pic, 0, 0, 0, 0, ax, ay,
					                 (unsigned)a.width, (unsigned)a.height);
				}
				XRenderFreePicture(dpy, pic);
			}
			XFreePixmap(dpy, pm);
		}
	}

children:
	{
		Window rr, *kids = NULL;
		unsigned int nk = 0;
		if (XQueryTree(dpy, node, &rr, &rr, &kids, &nk)) {
			/* XQueryTree children sırası: alttan üste */
			for (unsigned int i = 0; i < nk; i++)
				paint_subtree(c, kids[i], offx + a.x, offy + a.y, top, mask,
				              mx, my);
			if (kids)
				XFree(kids);
		}
	}
	(void)top;
}

/* Bir üst düzey pencereyi tüm efektleriyle çizer. */
static void paint_window(LCompositor *c, Window w, LCompW *e)
{
	Display *dpy = c->dpy;
	XWindowAttributes a;
	if (!XGetWindowAttributes(dpy, w, &a))
		return;
	if (a.map_state != IsViewable || a.width <= 0 || a.height <= 0)
		return;
	if (a.class == InputOnly)
		return;

	/* geometri önbelleğini tazele */
	e->x = a.x;
	e->y = a.y;
	e->wid = a.width;
	e->ht = a.height;
	e->ovr = a.override_redirect != 0;

	int W = a.width, H = a.height;
	int corner = e->dock ? 0 : c->cfg->comp.corner_radius;

	/* ---- alfa değerini belirle ---- */
	double g = 1.0;
	if (!e->dock && !e->ovr) {
		if (e->w != c->active)
			g = c->cfg->comp.inactive_opacity;
	}
	if (e->clopacity != 0xffffffffUL && e->clopacity != 0) {
		double cl = (double)e->clopacity / 65535.0;
		g *= cl;
	}

	/* ---- gölge ---- */
	if (!e->dock && !e->ovr && c->cfg->comp.shadow_radius > 0) {
		int sr = c->cfg->comp.shadow_radius;
		int key = sr * 100000 + W * 1000 + H;
		if (e->shadow.pm && e->shadow_key != key) {
			xsurf_free(dpy, &e->shadow);
			e->shadow.pm = None;
		}
		if (!e->shadow.pm) {
			e->shadow = xsurf_shadow(dpy, c->scr, W, H, sr,
			                         c->cfg->comp.shadow_opacity);
			e->shadow_key = key;
		}
		if (e->shadow.pm) {
			int pad = LMAX(sr, 2);
			int dy = 2 + sr / 4; /* aşağı offset */
			XRenderComposite(dpy, PictOpOver, e->shadow.pic, None,
			                 c->back.pic, 0, 0, 0, 0, a.x - pad,
			                 a.y - pad + dy, (unsigned)(W + 2 * pad),
			                 (unsigned)(H + 2 * pad));
		}
	}

	/* ---- reveal animasyonu ---- */
	Picture mask = None;
	int mask_offx = 0, mask_offy = 0;
	XSurf tmpmask;
	memset(&tmpmask, 0, sizeof(tmpmask));
	double rev = 1.0;
	if (tween_done(&e->rev)) {
		if (e->wants_close) {
			/* minimize animasyonu bitti → kaldır */
			e->wants_close = 0;
			e->rev.active = 0;
			XUnmapWindow(dpy, w);
			return;
		}
		e->rev.from = e->rev.to = 1.0;
	} else {
		rev = tween_value(&e->rev);
		if (rev <= 0.01)
			rev = 0.001;
		/* merkezden büyüyen dikdörtgen maskesi */
		if (rev < 1.0) {
			tmpmask = xsurf_reveal_mask(dpy, c->scr, W, H, rev);
			mask = tmpmask.pic;
			mask_offx = a.x;
			mask_offy = a.y;
		}
	}
	(void)rev;

	/* köşe + alfa maskesi (reveal yokken) */
	if (!mask) {
		int alpha_key = (int)(g * 255.0 + 0.5);
		int key = W * 1000000 + H * 1000 + corner * 4 + alpha_key;
		if (e->mask.pm && e->mask_key != key) {
			xsurf_free(dpy, &e->mask);
			e->mask.pm = None;
		}
		if (!e->mask.pm) {
			e->mask = xsurf_win_mask(dpy, c->scr, W, H, corner, g);
			e->mask_key = key;
		}
		if (e->mask.pm) {
			mask = e->mask.pic;
			mask_offx = a.x;
			mask_offy = a.y;
		}
	}

	/* ---- blur arka planı (yalnızca blur isteyen pencereler) ---- */
	if (e->blur && c->cfg->comp.blur_enabled) {
		int br = e->blur_r > 0 ? e->blur_r : c->cfg->comp.blur_radius;
		int bx = a.x - br, by = a.y - br;
		int bw2 = W + 2 * br, bh2 = H + 2 * br;
		/* kırp */
		int cx0 = LMAX(bx, 0), cy0 = LMAX(by, 0);
		int cx1 = LMIN(bx + bw2, c->sw), cy1 = LMIN(by + bh2, c->sh);
		if (cx1 > cx0 && cy1 > cy0) {
			int cw2 = cx1 - cx0, ch2 = cy1 - cy0;
			XSurf reg = xsurf_new_argb(dpy, c->scr, cw2, ch2);
			XRenderComposite(dpy, PictOpSrc, c->back.pic, None, reg.pic,
			                 cx0, cy0, 0, 0, 0, 0, (unsigned)cw2,
			                 (unsigned)ch2);
			XSurf blur = xsurf_box_blur(dpy, c->scr, reg, 0, 0, cw2, ch2,
			                            br);
			XRenderComposite(dpy, PictOpOver, blur.pic, None, c->back.pic,
			                 0, 0, 0, 0, cx0, cy0, (unsigned)cw2,
			                 (unsigned)ch2);
			xsurf_free(dpy, &blur);
			xsurf_free(dpy, &reg);
		}
	}

	/* ---- içerik ---- */
	paint_subtree(c, w, 0, 0, e, mask, mask_offx, mask_offy);

	if (tmpmask.pm)
		xsurf_free(dpy, &tmpmask);

	/* Not: 2px kenar çerçevesi, köşe maskesinden sonra dört kenar şeridi
	 * maske ile çizilebilir (bordür aktif/pasif renk). Kod isteğe bağlı
	 * bırakıldı; çerçevesiz + yuvarlak köşe + gölge görseli tasarımın
	 * "başlık çubuğu yok" ruhuna uygun olduğundan varsayılan kapalıdır.
	 */
	(void)corner;
}

/* Tüm sahneyi alttan üste çizer. */
static void paint_scene(LCompositor *c)
{
	Display *dpy = c->dpy;
	int sw = c->sw, sh = c->sh;

	/* 1) arka plan (wallpaper) */
	xsurf_clear(dpy, &c->back);
	XRenderComposite(dpy, PictOpOver, c->wall.pic, None, c->back.pic, 0, 0,
	                 0, 0, 0, 0, (unsigned)sw, (unsigned)sh);

	/* 2) alt pencere ağacını topla (root children, alttan üste) */
	Window rr, *kids = NULL;
	unsigned int nk = 0;
	if (!XQueryTree(dpy, c->root, &rr, &rr, &kids, &nk))
		return;

	for (unsigned int i = 0; i < nk; i++) {
		Window w = kids[i];
		LCompW *e = find_win(c, w);
		XWindowAttributes a;
		if (!XGetWindowAttributes(dpy, w, &a))
			continue;
		if (a.map_state != IsViewable || a.width <= 0 || a.height <= 0)
			continue;
		if (!e) {
			/* kayıtsız pencere (örn. açılış anı) — yine de çiz */
			e = win_add(c, w);
		}
		paint_window(c, w, e);
	}
	if (kids)
		XFree(kids);

	/* 3) workspace slide: eski sahneyi kaydır, yenisini sür */
	if (c->slide_on) {
		double p = tween_value(&c->slide_t);
		int off_old = (int)(-c->slide_dir * p * sw);
		int off_new = (int)(-c->slide_dir * (1.0 - p) * sw);

		/* yeni sahneyi önce temiz bir katmana kopyala (back'te) */
		/* kaydırma: back'i yeniden çizemeyeceğimiz için slide yalnızca
		 * "eski kayar, yeni görünür" şeklinde: snap kaydır + back sabit
		 * arka katman olarak çizilir. Bu gerçekçi bir yaklaşım için
		 * iki katmanlı çizim yapılır: */
		XSurf scene;
		memset(&scene, 0, sizeof(scene));
		scene = xsurf_new_argb(dpy, c->scr, sw, sh);
		XRenderComposite(dpy, PictOpSrc, c->back.pic, None, scene.pic, 0, 0,
		                 0, 0, 0, 0, (unsigned)sw, (unsigned)sh);

		/* back'i yeniden doldur: eski sahne + yeni sahne ofsetleri */
		xsurf_clear(dpy, &c->back);
		XRenderComposite(dpy, PictOpOver, c->wall.pic, None, c->back.pic,
		                 0, 0, 0, 0, 0, 0, (unsigned)sw, (unsigned)sh);
		/* eski sahne */
		XRenderComposite(dpy, PictOpOver, c->snap.pic, None, c->back.pic,
		                 off_old, 0, 0, 0, 0, 0, (unsigned)sw, (unsigned)sh);
		/* yeni sahne */
		XRenderComposite(dpy, PictOpOver, scene.pic, None, c->back.pic,
		                 off_new, 0, 0, 0, 0, 0, (unsigned)sw, (unsigned)sh);
		xsurf_free(dpy, &scene);

		if (tween_done(&c->slide_t)) {
			c->slide_on = 0;
			if (c->snap.pm) {
				xsurf_free(dpy, &c->snap);
				c->snap.pm = None;
			}
		}
	}

	/* 4) hedefe bas */
	XRenderComposite(dpy, PictOpSrc, c->back.pic, None, c->rootpic, 0, 0, 0,
	                 0, 0, 0, (unsigned)sw, (unsigned)sh);
	XFlush(dpy);
}

/* ------------------------------------------------------------------ */
/* Kare döngüsü                                                        */
/* ------------------------------------------------------------------ */

int comp_frame(LCompositor *c)
{
	/* animasyonları ilerlet */
	int animating = 0;
	for (LCompW *e = c->wins; e; e = e->next) {
		if (!e->freed && e->rev.active) {
			animating = 1;
			c->dirty = 1;
		}
	}
	if (c->slide_on) {
		animating = 1;
		c->dirty = 1;
		if (tween_done(&c->slide_t))
			c->slide_on = 0;
	}
	c->animating = animating;

	if (!c->dirty)
		return 0;

	comp_subtract_all(c);
	paint_scene(c);
	c->dirty = 0;
	return 1;
}

int comp_timeout_ms(LCompositor *c)
{
	if (c->dirty)
		return 0;              /* hemen çiz */
	if (c->animating || c->slide_on) {
		/* yaklaşık 60fps için kare süresi */
		double d = 16.6 - fmod(anim_now_ms(), 16.6);
		if (d < 1) d = 16.6;
		return (int)d;
	}
	return 250;                /* olaysız bekleme */
}

/* Wallpaper'ı yeniden yakala (SIGUSR1: feh'ten sonra). */
void comp_wall_recapture(LCompositor *c)
{
	XRenderComposite(c->dpy, PictOpOver, c->rootpic, None, c->wall.pic, 0, 0,
	                 0, 0, 0, 0, (unsigned)c->sw, (unsigned)c->sh);
	mark_full(c);
}
