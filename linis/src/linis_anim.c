/* linis_anim.c */
#define _POSIX_C_SOURCE 200809L

#include <time.h>
#include <math.h>

#include "linis_anim.h"

static double mono_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

double anim_now_ms(void)
{
	return mono_ms();
}

double anim_ease(int type, double t)
{
	if (t <= 0) return 0;
	if (t >= 1) return 1;
	double c;
	switch (type) {
	case EASE_LINEAR:     return t;
	case EASE_OUT_QUAD:   return t * (2 - t);
	case EASE_OUT_CUBIC:
		t = t - 1;
		return t * t * t + 1;
	case EASE_IN_OUT_CUBIC:
		return t < 0.5 ? 4 * t * t * t : 1 - pow(-2 * t + 2, 3) / 2;
	case EASE_OUT_BACK:
		c = 1.70158;
		{ double p = t - 1; return p * p * ((c + 1) * p + c) + 1; }
	default: return t;
	}
}

void tween_start(Tween *tw, int ease, double from, double to, double dur_ms)
{
	if (dur_ms <= 0) {
		tw->active = 0;
		tw->from = tw->to = to;
		return;
	}
	tw->active = 1;
	tw->ease = ease;
	tw->t0 = mono_ms();
	tw->dur = dur_ms;
	tw->from = from;
	tw->to = to;
}

int tween_done(Tween *tw)
{
	return !tw->active || (mono_ms() - tw->t0) >= tw->dur;
}

double tween_value(Tween *tw)
{
	if (!tw->active)
		return tw->to;
	double p = (mono_ms() - tw->t0) / tw->dur;
	if (p >= 1) {
		p = 1;
		tw->active = 0;
	}
	return tw->from + (tw->to - tw->from) * anim_ease(tw->ease, p);
}

double tween_update(Tween *tw, double *out)
{
	if (!tw->active) {
		*out = tw->to;
		return 1.0;
	}
	double p = (mono_ms() - tw->t0) / tw->dur;
	if (p >= 1) {
		p = 1;
		tw->active = 0;
	}
	*out = tw->from + (tw->to - tw->from) * anim_ease(tw->ease, p);
	return p;
}
