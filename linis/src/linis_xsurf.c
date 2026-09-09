/*
 * linis_xsurf.c
 *
 * ARGB32 tabanlı XRender yardımcıları: yüzey üretimi, bölge kopya,
 * kutusal blur (küçült-büyüt), alfa-kanalı CPU yumuşatması ve kenar
 * yumuşatmalı yuvarlak köşe maskesi.
 *
 * CPU'ya düşen piksel yazımlarında her byte aynı değerle doldurulur;
 * böylece X sunucusunun 32-bit görsel kanal sıralamasından bağımsız
 * olarak alfa byte'ı doğru yere düşer (Render ARGB32'de üst byte).
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "linis_xsurf.h"
#include "linis_util.h"

XSurf xsurf_new_argb(Display *dpy, int scr, int w, int h)
{
	XSurf s;
	memset(&s, 0, sizeof(s));
	s.w = w;
	s.h = h;
	s.pm = XCreatePixmap(dpy, RootWindow(dpy, scr), (unsigned)LMAX(w, 1),
	                     (unsigned)LMAX(h, 1), 32);
	s.fmt = XRenderFindStandardFormat(dpy, PictStandardARGB32);
	s.pic = XRenderCreatePicture(dpy, s.pm, s.fmt, 0, NULL);
	return s;
}

void xsurf_free(Display *dpy, XSurf *s)
{
	if (s->pic)
		XRenderFreePicture(dpy, s->pic);
	if (s->pm)
		XFreePixmap(dpy, s->pm);
	s->pic = None;
	s->pm = None;
}

void xsurf_clear(Display *dpy, XSurf *s)
{
	XRenderColor c = { 0, 0, 0, 0 };
	XRenderFillRectangle(dpy, PictOpSrc, s->pic, &c, 0, 0,
	                     (unsigned)s->w, (unsigned)s->h);
}

void xr_copy(Display *dpy, Picture src, Picture dst,
             int sx, int sy, int dx, int dy, int w, int h)
{
	XRenderComposite(dpy, PictOpOver, src, None, dst, sx, sy, 0, 0, dx, dy,
	                 (unsigned)w, (unsigned)h);
}

void xr_fill(Display *dpy, Picture dst, int x, int y, int w, int h,
             double alpha)
{
	if (w <= 0 || h <= 0)
		return;
	unsigned short a = (unsigned short)(alpha * 65535.0 + 0.5);
	XRenderColor c = { a, a, a, a };
	XRenderFillRectangle(dpy, PictOpOver, dst, &c, x, y, (unsigned)w,
	                     (unsigned)h);
}

/* Geçici ölçek transformu ile çizim (src bizim yüzeyimiz). */
static void composite_scaled(Display *dpy, Picture src, Picture dst,
                             int sx, int sy, int w, int h,
                             int dx, int dy, int dw, int dh)
{
	if (w <= 0 || h <= 0 || dw <= 0 || dh <= 0)
		return;
	XTransform t = {{
		{ XDoubleToFixed((double)dw / w), XDoubleToFixed(0),
		  XDoubleToFixed(dx - (double)sx * dw / w) },
		{ XDoubleToFixed(0), XDoubleToFixed((double)dh / h),
		  XDoubleToFixed(dy - (double)sy * dh / h) },
		{ XDoubleToFixed(0), XDoubleToFixed(0), XDoubleToFixed(1) },
	}};
	XTransform id = {{
		{ XDoubleToFixed(1), XDoubleToFixed(0), XDoubleToFixed(0) },
		{ XDoubleToFixed(0), XDoubleToFixed(1), XDoubleToFixed(0) },
		{ XDoubleToFixed(0), XDoubleToFixed(0), XDoubleToFixed(1) },
	}};
	XRenderSetPictureTransform(dpy, src, &t);
	XRenderSetPictureFilter(dpy, src, FilterBilinear, NULL, 0);
	XRenderComposite(dpy, PictOpOver, src, None, dst, sx, sy, 0, 0, dx, dy,
	                 (unsigned)dw, (unsigned)dh);
	XRenderSetPictureTransform(dpy, src, &id);
	XRenderSetPictureFilter(dpy, src, FilterNearest, NULL, 0);
}

XSurf xsurf_box_blur(Display *dpy, int scr, XSurf src,
                     int sx, int sy, int w, int h, int radius)
{
	if (radius <= 1) {
		XSurf out = xsurf_new_argb(dpy, scr, w, h);
		xr_copy(dpy, src.pic, out.pic, sx, sy, 0, 0, w, h);
		return out;
	}
	int k = LMAX(1, radius / 2);
	int sw = LMAX(1, w / k);
	int sh = LMAX(1, h / k);
	XSurf small = xsurf_new_argb(dpy, scr, sw, sh);
	composite_scaled(dpy, src.pic, small.pic, sx, sy, w, h, 0, 0, sw, sh);

	XSurf out = xsurf_new_argb(dpy, scr, w, h);
	if (radius >= 4) {
		int sw2 = LMAX(1, sw / 2);
		int sh2 = LMAX(1, sh / 2);
		XSurf mid = xsurf_new_argb(dpy, scr, sw2, sh2);
		composite_scaled(dpy, small.pic, mid.pic, 0, 0, sw, sh, 0, 0, sw2, sh2);
		composite_scaled(dpy, mid.pic, out.pic, 0, 0, sw2, sh2, 0, 0, w, h);
		xsurf_free(dpy, &mid);
	} else {
		composite_scaled(dpy, small.pic, out.pic, 0, 0, sw, sh, 0, 0, w, h);
	}
	xsurf_free(dpy, &small);
	return out;
}

Picture xr_picture_from_pixmap(Display *dpy, Pixmap pm, int depth,
                               Visual *vis)
{
	XRenderPictFormat *f = NULL;
	if (vis) {
		f = XRenderFindVisualFormat(dpy, vis);
	} else if (depth >= 32) {
		f = XRenderFindStandardFormat(dpy, PictStandardARGB32);
	} else {
		f = XRenderFindStandardFormat(dpy, PictStandardRGB24);
	}
	return XRenderCreatePicture(dpy, pm, f, 0, NULL);
}

/* ------------------------------------------------------------------ */
/* Maskeler (ARGB32; alfa = kapsama)                                  */
/* ------------------------------------------------------------------ */

/* Bir yüzeyi CPU piksel değerleriyle doldurur. words: w*h uzunluğunda,
 * her byte eşit (alfa değeri). */
static void xsurf_put_words(Display *dpy, int scr, Pixmap pm, int w, int h,
                            unsigned int *words)
{
	XImage *img = XCreateImage(dpy, NULL, 32, ZPixmap, 0, (char *)words, w, h,
	                           32, 0);
	if (!img) {
		free(words);
		return;
	}
	/* satır dolgusu olmadan yaz (bitmap_pad=32 → 4 byte katı zaten) */
	XPutImage(dpy, pm, DefaultGC(dpy, scr), img, 0, 0, 0, 0, w, h);
	XDestroyImage(img); /* words verisini de serbest bırakır */
}

/* Ortak maske kurucusu: köşe yarıçapı ve genel alfa. */
static XSurf mask_build(Display *dpy, int scr, int w, int h, int r,
                        double alpha)
{
	XSurf m = xsurf_new_argb(dpy, scr, w, h);
	if (w <= 0 || h <= 0)
		return m;
	int fa = (int)(alpha * 255.0 + 0.5);
	if (fa < 0) fa = 0;
	if (fa > 255) fa = 255;
	unsigned int fillv = (unsigned int)fa;
	fillv |= fillv << 8;
	fillv |= fillv << 16;
	fillv |= fillv << 24;
	unsigned int *px = xmalloc((size_t)w * h * sizeof(unsigned int));
	if (r <= 0 || r > w / 2 || r > h / 2) {
		for (int i = 0; i < w * h; i++)
			px[i] = fillv;
		xsurf_put_words(dpy, scr, m.pm, w, h, px);
		return m;
	}

	/* dıştaki alanlar: köşe kareleri dışında her yer (alfa ile) dolu */
	for (int y = 0; y < h; y++) {
		unsigned int *row = px + (size_t)y * w;
		for (int x = 0; x < w; x++) {
			int in_corner = (x < r && (y < r || y >= h - r)) ||
			                (x >= w - r && (y < r || y >= h - r));
			row[x] = in_corner ? 0x00000000u : fillv;
		}
	}

	/* köşe kapsaması (kenar yumuşatmalı) */
	for (int c = 0; c < 4; c++) {
		int ox = (c == 1 || c == 3) ? w - r : 0;
		int oy = (c >= 2) ? h - r : 0;
		int sx = (c == 1 || c == 3) ? -1 : 1; /* x yönü içeri */
		int sy = (c >= 2) ? -1 : 1;           /* y yönü içeri */
		for (int yy = 0; yy < r; yy++) {
			int Y = oy + (sy == 1 ? yy : (r - 1 - yy));
			for (int xx = 0; xx < r; xx++) {
				int X = ox + (sx == 1 ? xx : (r - 1 - xx));
				double dist = sqrt((double)(xx + 0.5) * (xx + 0.5) +
				                   (double)(yy + 0.5) * (yy + 0.5));
				double cov = LCLAMP((double)r - dist + 0.5, 0.0, 1.0);
				unsigned int a = (unsigned int)(cov * fa + 0.5);
				unsigned int v = a | (a << 8) | (a << 16) | (a << 24);
				px[(size_t)Y * w + X] = v;
			}
		}
	}
	xsurf_put_words(dpy, scr, m.pm, w, h, px);
	return m;
}

XSurf xsurf_rounded_mask(Display *dpy, int scr, int w, int h, int r)
{
	return mask_build(dpy, scr, w, h, r, 1.0);
}

XSurf xsurf_win_mask(Display *dpy, int scr, int w, int h, int r, double alpha)
{
	return mask_build(dpy, scr, w, h, r, alpha);
}

XSurf xsurf_reveal_mask(Display *dpy, int scr, int w, int h, double t)
{
	XSurf m = xsurf_new_argb(dpy, scr, w, h);
	xsurf_clear(dpy, &m);
	int rw = (int)(w * t + 0.5);
	int rh = (int)(h * t + 0.5);
	if (rw > 0 && rh > 0)
		xr_fill(dpy, m.pic, (w - rw) / 2, (h - rh) / 2, rw, rh, 1.0);
	return m;
}

/* ------------------------------------------------------------------ */
/* Yumuşak gölge                                                       */
/* ------------------------------------------------------------------ */

XSurf xsurf_shadow(Display *dpy, int scr, int w, int h, int radius,
                   double opacity)
{
	int pad = LMAX(radius, 2);
	int bw = w + 2 * pad;
	int bh = h + 2 * pad;
	XSurf out = xsurf_new_argb(dpy, scr, bw, bh);
	unsigned int *px = xcalloc((size_t)bw * bh, sizeof(unsigned int));
	unsigned int a0 = (unsigned int)(opacity * 255.0 + 0.5);
	unsigned int v0 = a0 | (a0 << 8) | (a0 << 16) | (a0 << 24);
	for (int y = pad; y < bh - pad; y++)
		for (int x = pad; x < bw - pad; x++)
			px[(size_t)y * bw + x] = v0;

	/* alfa kanalını CPU'da blurla; sonra geri yaz (tüm byte'lar eşit) */
	unsigned char *flat = xmalloc((size_t)bw * bh * 4);
	for (int i = 0; i < bw * bh; i++) {
		unsigned char a8 = (unsigned char)(px[i] & 0xff);
		flat[i * 4] = flat[i * 4 + 1] = flat[i * 4 + 2] = flat[i * 4 + 3] = a8;
	}
	free(px);

	int k = LMAX(1, radius / 3);
	unsigned char *srcp = xmalloc((size_t)bw * bh);
	unsigned char *tmp = xmalloc((size_t)bw * bh);
	for (int i = 0; i < bw * bh; i++)
		srcp[i] = flat[i * 4];
	for (int pass = 0; pass < 2; pass++) {
		for (int y = 0; y < bh; y++) {
			int y0 = LMAX(0, y - k), y1 = LMIN(bh - 1, y + k);
			for (int x = 0; x < bw; x++) {
				int acc = 0;
				int x0 = LMAX(0, x - k), x1 = LMIN(bw - 1, x + k);
				for (int yy = y0; yy <= y1; yy++) {
					const unsigned char *row = &srcp[yy * bw];
					for (int xx = x0; xx <= x1; xx++)
						acc += row[xx];
				}
				int cnt = (y1 - y0 + 1) * (x1 - x0 + 1);
				tmp[y * bw + x] = (unsigned char)(acc / cnt);
			}
		}
		memcpy(srcp, tmp, (size_t)bw * bh);
	}
	for (int i = 0; i < bw * bh; i++) {
		unsigned char a8 = srcp[i];
		flat[i * 4] = flat[i * 4 + 1] = flat[i * 4 + 2] = flat[i * 4 + 3] = a8;
	}
	free(srcp);
	free(tmp);

	XImage *img = XCreateImage(dpy, NULL, 32, ZPixmap, 0, (char *)flat, bw, bh,
	                           32, 0);
	if (img) {
		XPutImage(dpy, out.pm, DefaultGC(dpy, scr), img, 0, 0, 0, 0, bw, bh);
		XDestroyImage(img);
	} else {
		free(flat);
	}
	return out;
}
