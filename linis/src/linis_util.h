/* linis_util.h — bellek, log, string ve süreç yardımcıları */
#ifndef LINIS_UTIL_H
#define LINIS_UTIL_H

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

#define LMAX(a, b) ((a) > (b) ? (a) : (b))
#define LMIN(a, b) ((a) < (b) ? (a) : (b))
#define LCLAMP(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#define LARR_LEN(a) (sizeof(a) / sizeof((a)[0]))

void *xmalloc(size_t n);
void *xcalloc(size_t n, size_t sz);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);

void die(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));

/* Seviyeler: 0=hata 1=uyarı 2=bilgi 3=hata ayıklama */
void llog(int level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void llog_set_level(int level);

/* Komutu /bin/sh -c ile oturumdan kopuk çalıştır. Asla dönüşte bekletmez. */
void spawn(const char *cmd);

int  str_startswith(const char *s, const char *pre);
char *str_trim(char *s);            /* baştaki/sondaki boşlukları kırpar */
int  str_ieq(const char *a, const char *b);
void str_to_lower(char *s);

/* ~/.config/linis dizinini garanti eder; statik tamponda tam yol döner. */
char *linis_config_dir(void);
/* Sırasıyla: ~/.config/linis/<f>, /etc/xdg/linis/<f>, /usr/share/linis/<f>
 * ilk mevcut olanı döner (malloc). Yoksa NULL. */
char *linis_find_file(const char *f);
/* /usr/share/linis/<f> mutlak yol (malloc); var olmak zorunda değildir. */
char *linis_sysfile(const char *f);

int  mkdir_p(const char *path, int mode);

/* [[0..1]]'de iki rengi karıştır (RRGGBB). */
uint32_t color_mix(uint32_t a, uint32_t b, double t);

#endif
