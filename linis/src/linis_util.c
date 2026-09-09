/* linis_util.c */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "linis_util.h"

static int log_level = 2; /* varsayılan: INFO */

void *xmalloc(size_t n)
{
	void *p = malloc(n ? n : 1);
	if (!p)
		die("linis: bellek yetersiz (%zu bayt)", n);
	return p;
}

void *xcalloc(size_t n, size_t sz)
{
	void *p = calloc(n ? n : 1, sz ? sz : 1);
	if (!p)
		die("linis: bellek yetersiz (%zu x %zu)", n, sz);
	return p;
}

void *xrealloc(void *p, size_t n)
{
	void *q = realloc(p, n ? n : 1);
	if (!q)
		die("linis: bellek yetersiz (realloc %zu)", n);
	return q;
}

char *xstrdup(const char *s)
{
	if (!s)
		return NULL;
	size_t n = strlen(s) + 1;
	char *p = xmalloc(n);
	memcpy(p, s, n);
	return p;
}

char *xstrndup(const char *s, size_t n)
{
	char *p = xmalloc(n + 1);
	memcpy(p, s, n);
	p[n] = '\0';
	return p;
}

void die(const char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "linis: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(1);
}

void llog(int level, const char *fmt, ...)
{
	static const char *tag[] = { "HATA", "UYARI", "BILGI", "DBG" };
	va_list ap;
	if (level > log_level)
		return;
	fprintf(stderr, "[%s] ", tag[level & 3]);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

void llog_set_level(int level)
{
	log_level = level;
}

void spawn(const char *cmd)
{
	pid_t pid = fork();
	if (pid < 0) {
		llog(0, "fork basarisiz: %s", strerror(errno));
		return;
	}
	if (pid == 0) {
		setsid();
		int fd = open("/dev/null", O_RDWR);
		if (fd >= 0) {
			dup2(fd, 0); dup2(fd, 1); dup2(fd, 2);
			if (fd > 2) close(fd);
		}
		execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
		_exit(127);
	}
}

int str_startswith(const char *s, const char *pre)
{
	return strncmp(s, pre, strlen(pre)) == 0;
}

char *str_trim(char *s)
{
	char *end;
	while (*s && isspace((unsigned char)*s))
		s++;
	end = s + strlen(s);
	while (end > s && isspace((unsigned char)end[-1]))
		*--end = '\0';
	return s;
}

int str_ieq(const char *a, const char *b)
{
	while (*a && *b) {
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
		a++; b++;
	}
	return *a == *b;
}

void str_to_lower(char *s)
{
	for (; *s; s++)
		*s = (char)tolower((unsigned char)*s);
}

char *linis_config_dir(void)
{
	static char buf[1024];
	const char *home = getenv("HOME");
	if (!home)
		home = "/root";
	snprintf(buf, sizeof(buf), "%s/.config/linis", home);
	mkdir_p(buf, 0755);
	return buf;
}

char *linis_sysfile(const char *f)
{
	char *p = xmalloc(1024);
	snprintf(p, 1024, "/usr/share/linis/%s", f);
	return p;
}

char *linis_find_file(const char *f)
{
	static const char *base[] = {
		NULL, /* ~/.config/linis */
		"/etc/xdg/linis",
		"/usr/share/linis",
	};
	char path[1024];

	for (int i = 0; i < 3; i++) {
		if (i == 0)
			snprintf(path, sizeof(path), "%s/%s", linis_config_dir(), f);
		else
			snprintf(path, sizeof(path), "%s/%s", base[i], f);
		if (access(path, R_OK) == 0)
			return xstrdup(path);
	}
	return NULL;
}

int mkdir_p(const char *path, int mode)
{
	char tmp[1024];
	snprintf(tmp, sizeof(tmp), "%s", path);
	size_t len = strlen(tmp);
	if (!len)
		return 0;
	if (tmp[len - 1] == '/')
		tmp[len - 1] = '\0';
	for (char *p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			if (mkdir(tmp, mode) != 0 && errno != EEXIST)
				return -1;
			*p = '/';
		}
	}
	if (mkdir(tmp, mode) != 0 && errno != EEXIST)
		return -1;
	return 0;
}

uint32_t color_mix(uint32_t a, uint32_t b, double t)
{
	int ar = (a >> 16) & 0xff, ag = (a >> 8) & 0xff, ab = a & 0xff;
	int br = (b >> 16) & 0xff, bg = (b >> 8) & 0xff, bb = b & 0xff;
	int r = (int)(ar + (br - ar) * t + 0.5);
	int g = (int)(ag + (bg - ag) * t + 0.5);
	int bl = (int)(ab + (bb - ab) * t + 0.5);
	return ((uint32_t)(r & 0xff) << 16) | ((uint32_t)(g & 0xff) << 8)
	       | (uint32_t)(bl & 0xff);
}
