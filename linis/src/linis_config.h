/*
 * linis_config.h — Yapılandırma ve tema için ortak tipler.
 * linis, linis-panel ve linis-launch bu başlığı paylaşır.
 */
#ifndef LINIS_CONFIG_H
#define LINIS_CONFIG_H

#include <stdint.h>
#include <stddef.h>

/* ---- Tokyo Night (Storm) paleti; theme dosyasından override edilir ---- */
typedef struct {
	uint32_t bg_dark;      /* ana arka plan                  */
	uint32_t bg_mid;       /* panel arka planı               */
	uint32_t bg_light;     /* hover/seçili                   */
	uint32_t fg_primary;   /* ana yazı                       */
	uint32_t fg_dim;       /* soluk yazı                     */
	uint32_t accent_blue;
	uint32_t accent_purple;
	uint32_t accent_cyan;
	uint32_t accent_green;
	uint32_t accent_red;
	uint32_t accent_orange;
	uint32_t border_active;
	uint32_t border_normal;
} LColors;

/* ---- Compositor ayarları ---- */
typedef struct {
	int      shadow_radius;
	double   shadow_opacity;
	int      blur_radius;
	int      corner_radius;
	double   inactive_opacity;
	int      animation_ms;
	int      animations;
	int      blur_enabled;
	int      glow_enabled;
	double   glow_intensity;
	int      vsync;
} LComp;

typedef struct {
	LColors  colors;
	LComp    comp;

	/* wm / genel */
	int      outer_gap;
	int      inner_gap;
	int      master_ratio;      /* yüzde, 30..70 */
	int      workspace_count;   /* varsayılan 4  */
	const char *wallpaper;
	const char *term_cmd;       /* örn: xfce4-terminal */
	const char *file_cmd;       /* örn: thunar       */
	const char *launcher_cmd;   /* örn: linis-launch */

	/* sol dikey panel */
	int      left_width;
	double   panel_opacity;
	int      panel_blur;

	/* alt yatay bar */
	int      bar_height;

	/* fontlar */
	const char *font_ui;
	int      font_size;

	/* ayar dosyası yolları (hangisi etkinse) */
	char     cfg_path[512];
	char     theme_path[512];
} LConfig;

/* Geçerli renklerin hex string'i: "%06x" ile çıktı üretilebilir. */

void config_defaults(LConfig *c);

/* Config yükle: önce yerleşik varsayılan, sonra dosyalar (sıralı, üst üste biner).
 * pathler NULL bitebilir. Dönen: okunan dosya sayısı. */
int  config_load(LConfig *c, const char **paths, int n);

uint32_t parse_color(const char *s, uint32_t fallback);
int      parse_bool(const char *s, int fallback);
void     parse_color_premul(uint32_t rgb, double alpha, /*out*/ uint16_t *a,
                            uint16_t *r, uint16_t *g, uint16_t *b);

#endif
