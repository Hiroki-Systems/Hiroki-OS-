/* linis_keybind.h — kısayol çözümleme ve olay gönderimi */
#ifndef LINIS_KEYBIND_H
#define LINIS_KEYBIND_H

#include <X11/Xlib.h>

typedef void (*KBHandler)(const char *arg, void *user);

typedef struct KBinding {
	struct KBinding *next;
	KeySym   sym;
	unsigned mod;
	unsigned mod_ignore;   /* LockMask | Mod2Mask (NumLock) */
	KBHandler fn;
	const char *arg;       /* komut metni (örn. "spawn:xterm") */
	const char *desc;
} KBinding;

typedef struct LKeybinds LKeybinds;

/* Kayıtlı bağların başı (XGrabKey kurulumu için). */
KBinding *kb_bindings(LKeybinds *k);

LKeybinds *kb_new(void);
void       kb_free(LKeybinds *k);

/* "Mod4+Shift+t" / "Super+t" / "Control+Alt+t" çözümleyip kaydeder.
 * fn, tetiklenince arg ile çağrılır. Dönüş 0 = başarı. */
int  kb_add_str(LKeybinds *k, const char *desc, const char *str,
                KBHandler fn, const char *arg, void *user);

/* keybinds.conf yükler; her satır fn(arg = komut) şeklinde kaydedilir. */
int  kb_load_file(LKeybinds *k, const char *path, KBHandler fn, const char *arg,
                  void *user);

/* Yalnızca tek bir mod tuşuna (örn. Mod4) basılmasını izle. Mod tuşu
 * —başka tuşa basılmadan— bırakılırsa (ya da timeout_ms geçerse) fn
 * çalışır. (Launcher için Super tek basımı) */
void kb_watch_mod(LKeybinds *k, unsigned mod, double timeout_ms,
                  KBHandler fn, const char *arg);
/* Mod-watch tetiklendi mi kontrolü (her olay turunda). */
int  kb_watch_fire_pending(LKeybinds *k);
/* Zaman aşımı/release tabanlı mod-watch kontrolü (döngü tick'i). */
int  kb_tick(LKeybinds *k);

/* Her X olay turunda çağrılmalı. 1 = mod-watch tetiklendi. */
int  kb_tick(LKeybinds *k);

int  kb_handle(LKeybinds *k, XKeyEvent *ev);
void kb_refresh(LKeybinds *k);
void kb_timeout_reset(LKeybinds *k);

int  kb_mod_from_name(const char *name);

#endif
