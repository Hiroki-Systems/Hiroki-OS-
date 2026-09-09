/*
 * linis_keybind.c — kısayol parser'ı + X olay yönlendirici.
 *
 * keybinds.conf biçimi (LINIS_DESIGN.md §9, §10):
 *   # yorum
 *   mod4+t   spawn:xfce4-terminal      # opsiyonel açıklama
 *   mod4+1   workspace:1
 *
 * "mod4" == "super"; LockMask ve NumLock etkisizleştirilir.
 * "tuşlar = komut" iki yazımı da (boşluksuz/boşluklu) kabul edilir.
 *
 * Ek olarak "mod watch" desteklenir: sadece Super (Mod4) basılıp
 * bırakılan mod tuşu kısa süre sonra tetiklenir (launcher için).
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "linis_keybind.h"
#include "linis_util.h"
#include "linis_anim.h"

/* ------------------------------------------------------------------ */
/* Mod adları                                                          */
/* ------------------------------------------------------------------ */

struct ModName { const char *name; unsigned mask; };
static const struct ModName mod_names[] = {
	{ "mod4",    Mod4Mask    }, { "super", Mod4Mask    }, { "win", Mod4Mask },
	{ "mod1",    Mod1Mask    }, { "alt",   Mod1Mask    },
	{ "mod3",    Mod3Mask    }, { "mod5",  Mod5Mask    },
	{ "control", ControlMask }, { "ctrl",  ControlMask },
	{ "shift",   ShiftMask   }, { "lock",  LockMask    },
};

int kb_mod_from_name(const char *name)
{
	for (size_t i = 0; i < LARR_LEN(mod_names); i++)
		if (str_ieq(name, mod_names[i].name))
			return (int)mod_names[i].mask;
	return -1;
}

/* ------------------------------------------------------------------ */
/* Parser                                                              */
/* ------------------------------------------------------------------ */

static KeySym parse_keyname(const char *name)
{
	size_t len = strlen(name);
	if (len == 1) {
		char c = name[0];
		if (c >= 'a' && c <= 'z')
			c = (char)(c - 'a' + 'A');
		return (KeySym)c;
	}
	return XStringToKeysym(name);
}

static int parse_binding(const char *str, KeySym *sym, unsigned *mod,
                         unsigned *mod_ignore)
{
	char buf[512];
	char *parts[24];
	int n = 0;

	snprintf(buf, sizeof(buf), "%s", str);
	*mod = 0;
	*mod_ignore = LockMask | Mod2Mask;
	*sym = NoSymbol;

	char *save = NULL;
	for (char *tok = strtok_r(buf, "+ ", &save); tok; tok = strtok_r(NULL, "+ ", &save)) {
		if (n >= (int)LARR_LEN(parts))
			return -1;
		parts[n++] = tok;
	}
	if (n == 0)
		return -1;

	for (int i = 0; i < n - 1; i++) {
		int m = kb_mod_from_name(parts[i]);
		if (m < 0)
			return -1;
		*mod |= (unsigned)m;
	}
	*sym = parse_keyname(parts[n - 1]);
	return *sym != NoSymbol ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Kayıt listesi                                                       */
/* ------------------------------------------------------------------ */

struct LKeybinds {
	KBinding *list;
	/* mod-watch: yalnızca bir mod tuşu basılıp bırakıldığında tetiklenir */
	unsigned watch_mod;
	int      watch_armed;
	double   watch_armed_at;
	int      watch_any_key;
	KBHandler watch_fn;
	const char *watch_arg;
	double   watch_timeout_ms;
	int      fired_recently;
};

LKeybinds *kb_new(void)
{
	return xcalloc(1, sizeof(struct LKeybinds));
}

KBinding *kb_bindings(LKeybinds *k)
{
	return k->list;
}

void kb_free(LKeybinds *k)
{
	if (!k)
		return;
	KBinding *b = k->list;
	while (b) {
		KBinding *n = b->next;
		free((void *)b->arg);
		free((void *)b->desc);
		free(b);
		b = n;
	}
	free(k);
}

int kb_add_str(LKeybinds *k, const char *desc, const char *str,
               KBHandler fn, const char *arg, void *user)
{
	KeySym sym;
	unsigned mod, ignore;
	(void)user;
	if (parse_binding(str, &sym, &mod, &ignore))
		return -1;
	KBinding *b = xcalloc(1, sizeof(*b));
	b->sym = sym;
	b->mod = mod;
	b->mod_ignore = ignore;
	b->fn = fn;
	b->arg = xstrdup(arg ? arg : desc);
	b->desc = desc ? xstrdup(desc) : NULL;
	b->next = k->list;
	k->list = b;
	llog(3, "keybind: %s  ->  %s", str, b->arg ? b->arg : "");
	return 0;
}

/* keybinds.conf satırını çöz; 1 kayıt, 0 boş, -1 hata */
static int parse_conf_line(char *line, char *out_key, size_t ks,
                           char *out_cmd, size_t cs)
{
	char *s = str_trim(line);
	if (!*s || *s == '#')
		return 0;
	char *c = strchr(s, '#');
	if (c)
		*c = '\0';
	s = str_trim(s);
	char *eq = strchr(s, '=');
	if (eq)
		*eq = '\0';
	snprintf(out_key, ks, "%s", str_trim(s));
	snprintf(out_cmd, cs, "%s", str_trim(eq ? eq + 1 : ""));
	return (out_key[0] && out_cmd[0]) ? 1 : -1;
}

int kb_load_file(LKeybinds *k, const char *path, KBHandler fn, const char *arg,
                 void *user)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return -1;
	char line[1024];
	int ok = 0;
	while (fgets(line, sizeof(line), f)) {
		char key[256], cmd[512];
		int r = parse_conf_line(line, key, sizeof(key), cmd, sizeof(cmd));
		if (r == 0)
			continue;
		if (r < 0) {
			llog(1, "keybinds: hatali satir: %s", str_trim(line));
			continue;
		}
		if (kb_add_str(k, cmd, key, fn, arg ? arg : cmd, user) == 0)
			ok++;
		else
			llog(1, "keybinds: cozumlenemedi: '%s'", key);
	}
	fclose(f);
	llog(2, "keybinds: '%s' (%d kayit)", path, ok);
	return ok;
}

/* ------------------------------------------------------------------ */
/* Mod watch (sadece Super basımıyla launcher)                         */
/* ------------------------------------------------------------------ */

void kb_watch_mod(LKeybinds *k, unsigned mod, double timeout_ms,
                  KBHandler fn, const char *arg)
{
	k->watch_mod = mod;
	k->watch_timeout_ms = timeout_ms;
	k->watch_fn = fn;
	k->watch_arg = xstrdup(arg ? arg : "");
	k->watch_armed = 0;
	k->fired_recently = 0;
}

static void kb_watch_fire(struct LKeybinds *k)
{
	k->watch_armed = 0;
	k->fired_recently = 1;
	if (k->watch_fn)
		k->watch_fn(k->watch_arg, NULL);
}

/* Döngü her turda çağrılır; zaman aşımı (release kaçırıldıysa). */
int kb_tick(LKeybinds *k)
{
	if (!k->watch_armed)
		return 0;
	if (k->watch_any_key) {
		k->watch_armed = 0;
		return 0;
	}
	if (anim_now_ms() - k->watch_armed_at >= k->watch_timeout_ms) {
		kb_watch_fire(k);
		return 1;
	}
	return 0;
}

int kb_watch_fire_pending(LKeybinds *k)
{
	return kb_tick(k);
}

void kb_timeout_reset(LKeybinds *k)
{
	k->watch_armed = 0;
	k->watch_any_key = 0;
	k->fired_recently = 0;
}

/* Mod tuşu, izlenen modla eşleşiyor mu? */
static int mod_key_matches(KeySym sym, unsigned mod)
{
	if (mod & Mod4Mask)
		return sym == XK_Super_L || sym == XK_Super_R || sym == XK_Hyper_L ||
		       sym == XK_Hyper_R;
	if (mod & Mod1Mask)
		return sym == XK_Alt_L || sym == XK_Alt_R || sym == XK_Meta_L ||
		       sym == XK_Meta_R;
	if (mod & ControlMask)
		return sym == XK_Control_L || sym == XK_Control_R;
	if (mod & ShiftMask)
		return sym == XK_Shift_L || sym == XK_Shift_R;
	return 0;
}

/* ------------------------------------------------------------------ */
/* X olay işleme                                                       */
/* ------------------------------------------------------------------ */

static KeySym lookup_sym(XKeyEvent *ev, int *valid)
{
	*valid = 1;
	KeySym ks = XLookupKeysym(ev, 0);
	if (ks == NoSymbol)
		*valid = 0;
	return ks;
}

/* Tek harf keysym'lerinin büyük/küçük eşini eşle (klavye sütunları). */
static int sym_eq(KeySym a, KeySym b)
{
	if (a == b)
		return 1;
	if ((a >= 'a' && a <= 'z') && (b >= 'A' && b <= 'Z'))
		return a == b + 32;
	if ((a >= 'A' && a <= 'Z') && (b >= 'a' && b <= 'z'))
		return a == b - 32;
	return 0;
}

/* KeyPress işle. 1 = tüketildi. */
static int handle_press(struct LKeybinds *k, XKeyEvent *ev)
{
	int valid;
	KeySym sym = lookup_sym(ev, &valid);
	/* shift basılıysa sütun-1 sembolünü de dene */
	KeySym sym_shifted = NoSymbol;
	if (ev->state & ShiftMask)
		sym_shifted = XLookupKeysym(ev, 1);
	unsigned state_no_lock = ev->state & ~LockMask;

	/* izlenen mod tuşunun (örn. Super) tek basımı → watch kur */
	if (k->watch_fn && valid && mod_key_matches(sym, k->watch_mod)) {
		unsigned state_mods = state_no_lock & (ControlMask | Mod1Mask |
			Mod3Mask | Mod4Mask | Mod5Mask | ShiftMask);
		/* super zaten basılıysa (tekrar) ya da başka mod varken değil */
		if (!(state_no_lock & k->watch_mod) &&
		    (state_mods & ~k->watch_mod) == 0) {
			k->watch_armed = 1;
			k->watch_any_key = 0;
			k->watch_armed_at = anim_now_ms();
			return 1;
		}
	}

	/* arm edilmişken başka bir tuş kombinasyonu → watch iptal */
	if (k->watch_armed)
		k->watch_any_key = 1;

	/* Kayıtlı kombinasyonları dene */
	for (KBinding *b = k->list; b; b = b->next) {
		if (!b->fn)
			continue;
		unsigned state = ev->state & ~b->mod_ignore;
		if ((state & b->mod) != b->mod)
			continue;
		/* bağda olmayan fazladan basılı tuşlar → eşleşme yok */
		unsigned extra = state & ~b->mod;
		if (extra & (ShiftMask | ControlMask | Mod1Mask | Mod3Mask |
		             Mod4Mask | Mod5Mask))
			continue;
		if (!valid)
			continue;
		if (sym_eq(sym, b->sym) ||
		    (sym_shifted != NoSymbol && sym_eq(sym_shifted, b->sym))) {
			KBHandler fn = b->fn;
			const char *arg = b->arg;
			fn(arg, NULL);
			return 1;
		}
	}
	return 0;
}

int kb_handle(LKeybinds *k, XKeyEvent *ev)
{
	if (ev->type == KeyPress)
		return handle_press(k, ev);

	if (ev->type == KeyRelease && k->watch_armed) {
		int valid;
		KeySym sym = lookup_sym(ev, &valid);
		if (valid && mod_key_matches(sym, k->watch_mod) &&
		    !k->watch_any_key) {
			/* izlenen mod tuşu, başka tuşa basılmadan bırakıldı →
			 * launcher aç */
			kb_watch_fire(k);
			return 1;
		}
		if (k->watch_armed)
			k->watch_any_key = 1; /* karmaşık bırakma */
	}
	return 0;
}

void kb_refresh(LKeybinds *k)
{
	(void)k; /* keysym tabanlı çözümleme yeniden eşleme gerektirmez */
}
