/*
 * linis_config.c — Küçük TOML alt-kümesi parser'ı.
 *
 * Desteklenen söz dizimi (LINIS_DESIGN.md §9 ile uyumlu):
 *   [colors] bg_dark = "#1a1b26"
 *   [compositor] shadow_radius = 12  shadow_opacity = 0.3
 *   [panel_left] width = 48  opacity = 0.88
 *   [gaps] outer = 6
 *   [wm] workspaces = 4
 *   # yorumlar
 *
 * Dosyalar sırayla işlenir; sonraki dosya öncekinin üzerine biner.
 * Renkler "#rrggbb" / "#rrggbbaa" / "0xrrggbb" olabilir.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "linis_config.h"
#include "linis_util.h"

/* ------------------------------------------------------------------ */
/* Renk yardımcıları                                                   */
/* ------------------------------------------------------------------ */

static int hex_digit(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

uint32_t parse_color(const char *s, uint32_t fallback)
{
	const char *p = s;
	int v[8];
	int n = 0;

	if (!s || !*s)
		return fallback;
	if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
		p += 2;
	else if (p[0] == '#')
		p++;
	while (n < 8 && hex_digit(*p) >= 0)
		v[n++] = hex_digit(*p++);
	if (n != 6 && n != 8)
		return fallback;

	uint32_t rgb = 0;
	for (int i = 0; i < 6; i++)
		rgb = (rgb << 4) | (uint32_t)v[i];
	if (n == 8 && v[6] == 0 && v[7] == 0)
		return fallback; /* tamamen saydam renk anlamsız */
	return rgb;
}

int parse_bool(const char *s, int fallback)
{
	if (!s) return fallback;
	if (str_ieq(s, "true") || str_ieq(s, "yes") || str_ieq(s, "on") || strcmp(s, "1") == 0)
		return 1;
	if (str_ieq(s, "false") || str_ieq(s, "no") || str_ieq(s, "off") || strcmp(s, "0") == 0)
		return 0;
	return fallback;
}

void parse_color_premul(uint32_t rgb, double alpha, uint16_t *a, uint16_t *r,
                        uint16_t *g, uint16_t *b)
{
	*a = (uint16_t)(alpha * 65535.0 + 0.5);
	*r = (uint16_t)((((rgb >> 16) & 0xff) * alpha) * 257.0 + 0.5);
	*g = (uint16_t)((((rgb >> 8) & 0xff) * alpha) * 257.0 + 0.5);
	*b = (uint16_t)((((rgb >> 0) & 0xff) * alpha) * 257.0 + 0.5);
}

/* ------------------------------------------------------------------ */
/* Varsayılanlar (Tokyo Night). NOTE 7 gereği yalnızca geri dönüş      */
/* değeridir — gerçek değerler tema dosyasından gelir.                 */
/* ------------------------------------------------------------------ */

static void colors_default(LColors *c)
{
	c->bg_dark       = 0x1a1b26;
	c->bg_mid        = 0x24283b;
	c->bg_light      = 0x414868;
	c->fg_primary    = 0xc0caf5;
	c->fg_dim        = 0x565f89;
	c->accent_blue   = 0x7aa2f7;
	c->accent_purple = 0xbb9af7;
	c->accent_cyan   = 0x7dcfff;
	c->accent_green  = 0x9ece6a;
	c->accent_red    = 0xf7768e;
	c->accent_orange = 0xe0af68;
	c->border_active = 0x7aa2f7;
	c->border_normal = 0x24283b;
}

void config_defaults(LConfig *c)
{
	memset(c, 0, sizeof(*c));
	colors_default(&c->colors);

	c->comp.shadow_radius    = 12;
	c->comp.shadow_opacity   = 0.30;
	c->comp.blur_radius      = 8;
	c->comp.corner_radius    = 10;
	c->comp.inactive_opacity = 0.92;
	c->comp.animation_ms     = 300;
	c->comp.animations       = 1;
	c->comp.blur_enabled     = 1;
	c->comp.glow_enabled     = 1;
	c->comp.glow_intensity   = 0.35;
	c->comp.vsync            = 1;

	c->outer_gap      = 6;
	c->inner_gap      = 6;
	c->master_ratio   = 45;      /* %45 solda master */
	c->workspace_count = 4;
	c->term_cmd       = "alacritty";
	c->file_cmd       = "pcmanfm";
	c->launcher_cmd   = "rofi -show drun -show-icons";

	c->left_width   = 48;
	c->panel_opacity = 0.88;
	c->panel_blur   = 1;
	c->bar_height   = 32;

	c->font_ui      = "Sans";
	c->font_size    = 10;
}

/* ------------------------------------------------------------------ */
/* TOML alt kümesi parser                                              */
/* ------------------------------------------------------------------ */

static const char *skip_ws(const char *p)
{
	while (*p == ' ' || *p == '\t')
		p++;
	return p;
}

static int assign_color(uint32_t *dst, const char *val)
{
	uint32_t c = parse_color(val, 0xFFFFFFFFu);
	if (c == 0xFFFFFFFFu && val && val[0] != '#' && !str_startswith(val, "0x"))
		return -1;
	*dst = c;
	return 0;
}

/* Bir section+key çiftini LConfig üzerinde uygular. Dönüş: tanınan anahtar. */
static int apply_key(LConfig *c, const char *sec, const char *key,
                     const char *val)
{
	/* --- [colors] --- */
	if (strcmp(sec, "colors") == 0) {
		uint32_t *p = NULL;
		if (strcmp(key, "bg_dark") == 0)        p = &c->colors.bg_dark;
		else if (strcmp(key, "bg_mid") == 0)    p = &c->colors.bg_mid;
		else if (strcmp(key, "bg_light") == 0)  p = &c->colors.bg_light;
		else if (strcmp(key, "fg_primary") == 0)p = &c->colors.fg_primary;
		else if (strcmp(key, "fg_dim") == 0)    p = &c->colors.fg_dim;
		else if (strcmp(key, "accent_blue") == 0)   p = &c->colors.accent_blue;
		else if (strcmp(key, "accent_purple") == 0) p = &c->colors.accent_purple;
		else if (strcmp(key, "accent_cyan") == 0)   p = &c->colors.accent_cyan;
		else if (strcmp(key, "accent_green") == 0)  p = &c->colors.accent_green;
		else if (strcmp(key, "accent_red") == 0)    p = &c->colors.accent_red;
		else if (strcmp(key, "accent_orange") == 0) p = &c->colors.accent_orange;
		else if (strcmp(key, "border_active") == 0) p = &c->colors.border_active;
		else if (strcmp(key, "border_normal") == 0) p = &c->colors.border_normal;
		if (p) return assign_color(p, val);
		return 0;
	}

	/* --- [compositor] --- */
	if (strcmp(sec, "compositor") == 0) {
		if (strcmp(key, "shadow_radius") == 0)      c->comp.shadow_radius = atoi(val);
		else if (strcmp(key, "shadow_opacity") == 0)c->comp.shadow_opacity = atof(val);
		else if (strcmp(key, "blur_radius") == 0)   c->comp.blur_radius = atoi(val);
		else if (strcmp(key, "corner_radius") == 0) c->comp.corner_radius = atoi(val);
		else if (strcmp(key, "inactive_opacity") == 0)c->comp.inactive_opacity = atof(val);
		else if (strcmp(key, "animation_duration_ms") == 0) c->comp.animation_ms = atoi(val);
		else if (strcmp(key, "animations") == 0)    c->comp.animations = parse_bool(val, 1);
		else if (strcmp(key, "blur") == 0)          c->comp.blur_enabled = parse_bool(val, 1);
		else if (strcmp(key, "blur_enabled") == 0)  c->comp.blur_enabled = parse_bool(val, 1);
		else if (strcmp(key, "glow") == 0)          c->comp.glow_enabled = parse_bool(val, 1);
		else if (strcmp(key, "glow_intensity") == 0)c->comp.glow_intensity = atof(val);
		else if (strcmp(key, "vsync") == 0)         c->comp.vsync = parse_bool(val, 1);
		else return 0;
		return 1;
	}

	/* --- [panel_left] --- */
	if (strcmp(sec, "panel_left") == 0 || strcmp(sec, "panel") == 0) {
		if (strcmp(key, "width") == 0)       c->left_width = atoi(val);
		else if (strcmp(key, "opacity") == 0)c->panel_opacity = atof(val);
		else if (strcmp(key, "blur") == 0)   c->panel_blur = parse_bool(val, 1);
		else return 0;
		return 1;
	}

	/* --- [panel_bottom] / [bar] --- */
	if (strcmp(sec, "panel_bottom") == 0 || strcmp(sec, "bar") == 0) {
		if (strcmp(key, "height") == 0)      c->bar_height = atoi(val);
		else if (strcmp(key, "opacity") == 0)c->panel_opacity = atof(val);
		else if (strcmp(key, "blur") == 0)   c->panel_blur = parse_bool(val, 1);
		else return 0;
		return 1;
	}

	/* --- [gaps] --- */
	if (strcmp(sec, "gaps") == 0) {
		if (strcmp(key, "outer") == 0) c->outer_gap = atoi(val);
		else if (strcmp(key, "inner") == 0) c->inner_gap = atoi(val);
		else return 0;
		return 1;
	}

	/* --- [wm] / [general] --- */
	if (strcmp(sec, "wm") == 0 || strcmp(sec, "general") == 0) {
		if (strcmp(key, "workspaces") == 0 || strcmp(key, "workspace_count") == 0)
			c->workspace_count = LCLAMP(atoi(val), 1, 9);
		else if (strcmp(key, "master_ratio") == 0)
			c->master_ratio = LCLAMP(atoi(val), 30, 70);
		else if (strcmp(key, "outer_gap") == 0) c->outer_gap = atoi(val);
		else if (strcmp(key, "inner_gap") == 0) c->inner_gap = atoi(val);
		else if (strcmp(key, "wallpaper") == 0) { c->wallpaper = xstrdup(val); }
		else if (strcmp(key, "term") == 0)      { c->term_cmd = xstrdup(val); }
		else if (strcmp(key, "file_manager") == 0) { c->file_cmd = xstrdup(val); }
		else if (strcmp(key, "launcher") == 0)  { c->launcher_cmd = xstrdup(val); }
		else if (strcmp(key, "font") == 0)      { c->font_ui = xstrdup(val); }
		else if (strcmp(key, "font_size") == 0) { c->font_size = atoi(val); }
		else return 0;
		return 1;
	}

	return 0;
}

/* Bir TOML dosyasını okuyup c üzerine uygula. */
static int parse_toml_file(LConfig *c, const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return 0;

	char line[1024];
	char sec[64] = "";
	int applied = 0;

	while (fgets(line, sizeof(line), f)) {
		char *s = line;
		/* yorum ve EOL */
		char *hash = strchr(s, '#');
		if (hash) *hash = '\0';
		s = str_trim(s);
		if (!*s)
			continue;

		if (*s == '[') {
			char *close = strchr(s, ']');
			if (!close)
				continue;
			*close = '\0';
			snprintf(sec, sizeof(sec), "%s", str_trim(s + 1));
			continue;
		}

		char *eq = strchr(s, '=');
		if (!eq)
			continue;
		*eq = '\0';
		char *key = str_trim(s);
		char *val = str_trim(eq + 1);

		/* tırnaklı string değerleri söker */
		if (*val == '"' || *val == '\'') {
			size_t l = strlen(val);
			if (l >= 2 && (val[l - 1] == '"' || val[l - 1] == '\'')) {
				val[l - 1] = '\0';
				val++;
			}
		}
		if (!*key)
			continue;
		if (apply_key(c, sec, key, val))
			applied++;
	}
	fclose(f);
	llog(2, "config: '%s' okundu (%d anahtar)", path, applied);
	return applied > 0;
}

int config_load(LConfig *c, const char **paths, int n)
{
	int loaded = 0;
	for (int i = 0; i < n; i++) {
		if (paths[i] && access(paths[i], R_OK) == 0) {
			if (parse_toml_file(c, paths[i]))
				loaded++;
		}
	}
	return loaded;
}
