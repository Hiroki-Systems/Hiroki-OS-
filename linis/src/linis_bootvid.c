/*
 * linis_bootvid.c — açılış videosu oynatıcı.
 *
 * Tasarım:
 *  - Varsayılan klasör: /usr/share/linis/videos
 *  - Aynı config'de özelleştirilebilir:
 *      [video]
 *      dir = "/path/to/videos"        # aranacak klasör
 *      duration_sec = 12              # intro üst sınırı
 *  - Oynatıcı tercihi: mpv (varsa), değilse vlc, değilse ffplay.
 *  - mpv tam ekran isteği WM tarafından _NET_WM_STATE ClientMessage ile
 *    geldiği için WM'imiz bunu onaylar; video bittiğinde mpv kapanır.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <unistd.h>

#include "linis_bootvid.h"
#include "linis_util.h"

#define DEFAULT_VIDEO_DIR "/usr/share/linis/videos"

/* ------------------------------------------------------------------ */
/* Config'ten [video] anahtarlarını oku (öncelik: kullanıcı > sistem). */
/* ------------------------------------------------------------------ */

static void read_video_config(char *dir_out, size_t dirsz, int *duration)
{
	snprintf(dir_out, dirsz, "%s", DEFAULT_VIDEO_DIR);
	*duration = 12;

	char *path = linis_find_file("config.toml");
	if (!path)
		return;
	FILE *fp = fopen(path, "r");
	free(path);
	if (!fp)
		return;
	char line[512];
	int invideo = 0;
	while (fgets(line, sizeof(line), fp)) {
		char *s = line;
		char *hash = strchr(s, '#');
		if (hash)
			*hash = '\0';
		s = str_trim(s);
		if (!*s)
			continue;
		if (*s == '[') {
			invideo = strncmp(s, "[video]", 7) == 0;
			continue;
		}
		if (!invideo)
			continue;
		char *eq = strchr(s, '=');
		if (!eq)
			continue;
		*eq = '\0';
		char *key = str_trim(s);
		char *val = str_trim(eq + 1);
		if (*val == '"' || *val == '\'') {
			size_t l = strlen(val);
			if (l >= 2 && (val[l - 1] == '"' || val[l - 1] == '\''))
				val[l - 1] = '\0', val++;
		}
		if (strcmp(key, "dir") == 0 && *val)
			snprintf(dir_out, dirsz, "%s", val);
		else if (strcmp(key, "duration_sec") == 0)
			*duration = atoi(val);
	}
	fclose(fp);
	if (*duration < 3)
		*duration = 3;
	if (*duration > 120)
		*duration = 120;
}

/* ------------------------------------------------------------------ */
/* Klasördeki ilk video dosyası                                        */
/* ------------------------------------------------------------------ */

static int has_video_ext(const char *name)
{
	static const char *exts[] = { ".mp4", ".webm", ".mkv", ".avi",
		                          ".mov", ".m4v", ".ogv" };
	size_t l = strlen(name);
	for (size_t i = 0; i < LARR_LEN(exts); i++) {
		size_t el = strlen(exts[i]);
		if (l > el && strcasecmp(name + l - el, exts[i]) == 0)
			return 1;
	}
	return 0;
}

static int cmp_str(const void *a, const void *b)
{
	const char *const *x = a;
	const char *const *y = b;
	return strcmp(*x, *y);
}

static char *first_video_in(const char *dir)
{
	DIR *d = opendir(dir);
	if (!d)
		return NULL;
	char **found = NULL;
	int n = 0, cap = 0;
	struct dirent *de;
	while ((de = readdir(d))) {
		if (!has_video_ext(de->d_name))
			continue;
		if (n == cap) {
			cap = cap ? cap * 2 : 8;
			found = xrealloc(found, (size_t)cap * sizeof(char *));
		}
		found[n++] = xstrdup(de->d_name);
	}
	closedir(d);
	if (!n) {
		free(found);
		return NULL;
	}
	qsort(found, (size_t)n, sizeof(char *), cmp_str);
	char *full = xmalloc(1024);
	snprintf(full, 1024, "%s/%s", dir, found[0]);
	for (int i = 0; i < n; i++)
		free(found[i]);
	free(found);
	return full;
}

/* ------------------------------------------------------------------ */
/* Oynatma                                                             */
/* ------------------------------------------------------------------ */

void bootvid_play(const char *arg, void *user)
{
	(void)user;
	char dir[1024];
	int duration;
	read_video_config(dir, sizeof(dir), &duration);
	(void)arg; /* klasör ve süre config'ten gelir */

	char *video = first_video_in(dir);
	if (!video) {
		llog(2, "boot_video: '%s' icinde video bulunamadi", dir);
		free(video);
		return;
	}
	llog(2, "boot_video: %s (max %d sn)", video, duration);

	char cmd[2048];
	if (access("/usr/bin/mpv", X_OK) == 0 ||
	    access("/bin/mpv", X_OK) == 0) {
		/* tam ekran, sessiz; en fazla duration sn oynatır (kısa intro),
		 * bitince (ya da EOF'te) mpv kendini kapatır */
		snprintf(cmd, sizeof(cmd),
		         "exec mpv --fs --mute --no-terminal --no-window-dragging "
		         "--keep-open=no --end=%d \"%s\" &",
		         duration, video);
	} else if (access("/usr/bin/vlc", X_OK) == 0) {
		snprintf(cmd, sizeof(cmd),
		         "exec vlc --intf dummy --fullscreen --no-video-title-show "
		         "--play-and-exit \"%s\" &",
		         video);
	} else {
		snprintf(cmd, sizeof(cmd),
		         "exec ffplay -fs -autoexit -loglevel quiet \"%s\" &",
		         video);
	}
	/* yedek: player yoksa komut çalışmaz, sorun değil */
	spawn(cmd);
	free(video);
}
