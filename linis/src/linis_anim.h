/* linis_anim.h — küçük easing/tween motoru (compositor animasyonları) */
#ifndef LINIS_ANIM_H
#define LINIS_ANIM_H

#include <stdint.h>

enum {
	EASE_LINEAR,
	EASE_OUT_CUBIC,    /* minimize/open 0.3s ease-out  */
	EASE_IN_OUT_CUBIC, /* workspace geçişi 0.25s       */
	EASE_OUT_BACK,     /* launcher scale-up 0.2s       */
	EASE_OUT_QUAD,
};

/* [0..1] ilerlemeyi dönüştürür */
double anim_ease(int type, double t);

/* Milisaniye cinsinden monoton saat */
double anim_now_ms(void);

typedef struct {
	int      active;
	int      ease;
	double   t0;       /* başlangıç ms */
	double   dur;      /* süre ms     */
	double   from, to; /* değer aralığı */
} Tween;

void tween_start(Tween *tw, int ease, double from, double to, double dur_ms);
double tween_value(Tween *tw);       /* geçerli değer (bitince `to`) */
int tween_done(Tween *tw);
double tween_update(Tween *tw, double *out); /* ilerleme 0..1 + değer */

#endif
