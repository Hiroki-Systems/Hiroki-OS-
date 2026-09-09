# Linis Window Manager

Tokyo Night tarzı, minimal ama "fancy", X11 tabanlı bir window manager.
Built-in XRender compositor (gölge, yuvarlak köşe, blur, animasyonlar),
tiling/floating hibrit düzen, sol dikey panel + alt yatay bar ve uygulama
başlatıcı ile gelir. `LINIS_DESIGN.md` şartnamesinin uygulamasıdır.

## Özellikler

- **Modlar:** Floating (varsayılan) · Tiling (master+stack) · Fullscreen ·
  Monocle
- **4 workspace** (Super+1..4), geçişte slide animasyonu
- **Built-in compositor:** XRender tabanlı — yumuşak gölge, 10px yuvarlak
  köşe, kutu-blur (arka plan), aktif/pasif opaklık, açılış/kapanış scale
  animasyonu, workspace slide
- **Sol panel + alt bar** (`linis-panel`, tek process): workspace
  göstergesi, taskbar, saat, ses/ağ/pil/güç ikonları, güç menüsü
- **Launcher** (`linis-launch`): Super tuşuna basıp bırakın, yazın, Enter
- EWMH/ICCCM uyumluluğu (client list, active window, workarea, strut…)
- Tema renkleri **hardcoded değil** — `theme.toml`/`config.toml` (TOML)
- GTK/Qt yok; tek C kütüphaneleri: X11 ailesi + fontconfig/freetype

## Derleme ve Kurulum

```sh
# bağımlılıklar (Arch)
sudo pacman -S libx11 libxrender libxcomposite libxdamage libxfixes \
               libxext libxrandr libxinerama libxft fontconfig freetype2

make            # linis, linis-panel, linis-launch
sudo make install
```

Arch için kök dizindeki `PKGBUILD` kullanılabilir.

## Oturum

Ekran yöneticisi (LightDM vb.) menüsünden **Linis** seçin; ya da elle:

```sh
xinit /usr/bin/linis-session
```

`linis-session` duvar kağıdını önce kurar (compositor ilk karede root'u
wallpaper katmanı olarak yakalar), ardından WM + autostart'ı başlatır.

## Kısayollar (varsayılan)

| Tuş | Eylem |
|---|---|
| Super (bas-bırak) | Launcher |
| Super+T | Tiling / Floating |
| Super+Q | Pencereyi kapat |
| Super+F | Fullscreen |
| Super+M | Monocle |
| Super+Space | Pencereyi float yap |
| Super+J / K | Sonraki / önceki pencere |
| Super+H / L | Master alanını küçült / büyüt |
| Super+1..4 | Workspace geçişi |
| Super+Shift+1..4 | Pencereyi workspace'e taşı |
| Ctrl+Alt+T / Ctrl+Alt+F | Terminal / dosya yöneticisi |
| Super+V | Açılış (boot) videosunu oynat |
| Super+Shift+R / Q | WM yeniden başlat / çık |

Fare: **Super+Sol-sürükle** taşı, **Super+Sağ-sürükle** boyutlandır.
Düzen `~/.config/linis/keybinds.conf` ile özelleştirilir.

## Yapılandırma

Öncelik sırası (sonraki dosyalar öncekilerin üzerine biner):

```
~/.config/linis/config.toml      # kullanıcı (tavsiye)
/etc/xdg/linis/config.toml       # sistem
~/.config/linis/theme.toml       # renk + compositor (tema)
/usr/share/linis/themes/tokyo-night.toml
~/.config/linis/keybinds.conf    # kısayollar
~/.config/linis/autostart        # oturum başında çalışan uygulamalar
```

İlk kurulum:

```sh
mkdir -p ~/.config/linis
cp /etc/xdg/linis/config.toml            ~/.config/linis/
cp /usr/share/linis/themes/tokyo-night.toml ~/.config/linis/theme.toml
cp /usr/share/linis/keybinds.conf         ~/.config/linis/
cp /usr/share/linis/autostart            ~/.config/linis/
```

Duvar kağıdını değiştirip compositor'e bildirme:

```sh
feh --no-fehbg --bg-fill ~/resim.png && kill -USR1 $(pgrep -x linis)
```

## Açılış videosu

WM her başladığında (ya da **Super+V** ile) bir boot video oynatılabilir:

1. Videoyu sisteme koy: `sudo mkdir -p /usr/share/linis/videos && sudo cp ornek.mp4 /usr/share/linis/videos/`
2. İstersen `~/.config/linis/config.toml` içinde özelleştir:

```toml
[video]
dir          = "/usr/share/linis/videos"   # hangi klasörden seçilsin
duration_sec = 12                          # intro üst sınırı (sn)
```

3. WM'i yeniden başlat (`Super+Shift+R`), oturum açılırken video tam ekran +
   sessiz oynar; süre dolar ya da video biterse mpv kapanır ve masaüstü
   açılır. `q`/`Esc` ile de atlanabilir.

Oynatıcı: sistemde **mpv** varsa o kullanılır; yoksa vlc → ffplay. Klasörde
birden çok video varsa alfabetik ilki alınır (`.mp4 .webm .mkv .avi`…).
Not: mpv tam ekran isteği WM'in `_NET_WM_STATE` desteğiyle çalışır — video
tam ekranda açılır.

## Notlar / Bilinen Sınırlar

- Compositor **tam kare** yeniden çizim yapar (damage olduğunda); idle'da
  hiç iş yapmaz. Bölgesel (damage-area) yeniden çizim ileride eklenebilir.
- Pencere "gölgesi", CPU kutu-blur ile üretilir ve yalnızca pencere
  geometrisi değişince yeniden hesaplanır; çok büyük pencerelerde ilk açılış
  birkaç ms sürebilir (XRender yüzey kopyalarıyla optimize edilebilir).
- Bildirim/tepsi uygulamaları (dunst, nm-applet…) sisteme aittir; panel
  tepsi ikonları yalnızca ilgili uygulamayı açar.
- Ses/pil/ağ değerleri: pil `/sys`'ten okunur; ses/ağ seviyesi
  okunmaz (tıklama uygulamayı açar).
- XRender tabanlıdır (GLX yok) — zayıf/GPU'suz X'te de çalışır.
