/* linis_bootvid.h — açılış (boot) videosu yardımcıları */
#ifndef LINIS_BOOTVID_H
#define LINIS_BOOTVID_H

/* /usr/share/linis/videos (veya config'te belirtilen klasör) içindeki ilk
 * videoyu mpv ile tam ekran + sessiz oynatır. Video bitince/kullanıcı
 * çıkınca process kapanır, masaüstü görünür.
 *
 * WM kısayolu (Super+V) ya da config anahtarı olarak "boot_video"
 * kullanılabilir. */
void bootvid_play(const char *arg, void *user);

#endif
