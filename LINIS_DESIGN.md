# Linis Window Manager — Tasarım Dokümanı v1.0

## Genel Bakış
Linis, Hiroki OS için sıfırdan yazılmış, Tokyo Night rice tarzında, minimal ama fancy bir X11 window manager'dır. Built-in compositor, hibrit tiling/floating desteği, sol dikey panel + alt yatay bar ile gelir.

---

## 1. Mimari

```
linis (ana process)
├── X11 Connection (Xlib veya XCB)
├── EWMH/ICCCM Uyumluluğu
├── Built-in Compositor (XRender)
├── Window Management (floating + tiling)
├── Keybind Sistemi
└── Panel Daemon (ayrı process: linis-panel)
```

**Tek bağımlılıklar:** libX11, libXrender, libXcomposite, libXdamage, libXfixes, libXext, libXrandr, libXinerama, libXft, fontconfig, freetype2, Standard C lib

**Kullanılmayacak:** GTK, Qt, gettext, veya herhangi bir high-level toolkit

---

## 2. Renk Paleti (Tokyo Night)

```c
// Tokyo Night Storm
#define BG_DARK       0x1a1b26  // Ana arka plan
#define BG_MID        0x24283b  // Panel arka planı
#define BG_LIGHT      0x414868  // Hover/seçili
#define FG_PRIMARY    0xc0caf5  // Ana yazı rengi
#define FG_DIM        0x565f89  // Soluk yazı
#define ACCENT_BLUE   0x7aa2f7  // Mavi vurgu
#define ACCENT_PURPLE 0xbb9af7  // Mor vurgu
#define ACCENT_CYAN   0x7dcfff  // Cyan vurgu
#define ACCENT_GREEN  0x9ece6a  // Yeşil (aktif)
#define ACCENT_RED    0xf7768e  // Kırmızı (kapat)
#define ACCENT_ORANGE 0xe0af68  // Turuncu
#define BORDER_ACTIVE 0x7aa2f7  // Aktif pencere çerçeve
#define BORDER_NORMAL 0x24283b  // Pasif pencere çerçeve
#define SHADOW_COLOR  0x000000  // Gölge rengi (%30 opacity)
```

---

## 3. Pencere Yönetimi

### 3.1 Modlar
- **Floating (varsayılan):** Pencereler serbest hareket eder, boyutlandırılabilir
- **Tiling:** Pencereler otomatik döşenir, gap'li, ekranı böler
- **Fullscreen:** Tek pencere tüm ekranı kaplar
- **Monocle:** Tüm pencereler üst üste, sadece biri görünür

`Super+T` ile tiling/floating arası geçiş (config'den değiştirilebilir)

### 3.2 Tiling Layout
```
+------+--------+
|      |        |
|  1   |   2    |   ← Ana layout: sol %33, sağ %67
|      |        |
+------+--------+
|       3        |   ← Üçüncü pencere altta
+----------------+
```
- Gap: 6px (tüm kenarlarda, panel alanları hariç)
- Master area: sol tarafta, %33-67 arası ayarlanabilir (`Super+H`/`Super+L`)
- Stack area: sağ tarafta, dikey olarak bölünür

### 3.3 Pencere Dekorasyonu
- **Başlık çubuğu yok** (minimal)
- **Çerçeve:** 2px, aktif=pasif renk farkı
- **Gölge:** 12px radius, %30 opacity (compositor tarafından çizilir)
- **Yuvarlak köşe:** 10px radius (compositor)
- Başlık bilgisi tooltip olarak `Super+Hover` ile görünür

### 3.4 EWMH Uyumluluğu
- `_NET_WM_NAME`, `_NET_WM_CLASS` → pencere tanımlama
- `_NET_WM_STATE` → maximized, fullscreen, above, sticky
- `_NET_WM_WINDOW_TYPE` → dialog, splash, dock, toolbar
- `_NET_WORKSPACE` → workspace desteği (4 adet varsayılan)
- `_NET_CLIENT_LIST` → panel tarafından okunur
- `_NET_WM_DESKTOP` → workspace atama
- `_NET_WM_PID` → process tanımlama
- `_NET_WM_ICON` → uygulama ikonları (panel için)

### 3.5 ICCCM Uyumluluğu
- `WM_DELETE_WINDOW` → pencere kapatma
- `WM_PROTOCOLS` → destroy, take_focus
- `WM_HINTS` → input, initial_state
- `WM_NORMAL_HINTS` → min/max boyut, aspect ratio
- `WM_CLASS` → uygulama tanımlama
- `WM_COLORMAP_WINDOWS` → renk haritası

---

## 4. Compositor

### 4.1 Teknoloji
- XRender tabanlı (GLX değil, daha az bağımlılık)
- Back-buffer ile redraw
-amage tracking (XDamage) ile performans

### 4.2 Efektler
| Efekt | Varsayılan | Ayarlanabilir |
|-------|-----------|---------------|
| Gölge | Açık, 12px, %30 | Radius, offset, opacity |
| Blur | Açık, 8px | Radius, dye |
| Transparency | Kapalı | Seffaflik miktarı |
| Animasyon | Açık | Süre, easing |
| Yuvarlak köşe | Açık, 10px | Radius |
| Active glow | Açık | Renk, yoğunluk |

### 4.3 Pencere Durumuna Göre Efekt
- **Aktif pencere:** Full opacity, 2px mavi çerçeve, 12px gölge, 10px yuvarlak köşe
- **Pasif pencere:** %92 opacity, 2px koyu çerçeve, 8px gölge
- **Minimize:** Scale-down animasyonu (0.3s ease-out)
- **Open/Unminimize:** Scale-up animasyonu (0.3s ease-out)
- **Workspace geçişi:** Slide animasyonu (0.25s ease-in-out)
- **Hover (tooltip):** Fade-in 0.15s

### 4.4 Blur
- Arka plan blur: Pencere arka planı bulanıklaştırılır (arka plandaki duvar kağıdı/uygulamalar)
- Teknik: XRender Gaussian blur, 8px radius
- Performans: Yalnızca blur gerektiren pencereler için hesaplanır (`.ob-transparent` veya EWMH ile belirtilen pencereler)

---

## 5. Sol Dikey Panel (linis-panel)

### 5.1 Konum ve Boyut
- Sol kenar, sol alt köşeden başlar
- Genişlik: 48px
- Yükseklik: Ekran yüksekliği - alt bar yüksekliği
- Arka plan: `%88 opacity, blur 12px, Tokyo Night BG_MID`
- Kenar: Sağda 1px çizgi (ACCENT_BLUE)

### 5.2 İçerik (yukarıdan aşağıya)
```
┌──────────┐
│   LOGO   │  ← Hiroki/Linis logosu (24x24 PNG/SVG)
├──────────┤
│    1     │  ← Workspace göstergesi (aktif=aktif renk, pasif=dim)
│    2     │
│    3     │
│    4     │
├──────────┤
│  📁 📷  │  ← Çalışan uygulama ikonları (hover'da tooltip isim)
│  🌐 💬  │
│  ...     │
├──────────┤
│   ⚙️    │  ← Sistem (Ayarlar, Güç menüsü)
└──────────┘
```

### 5.3 Davranış
- Workspace numaralarına tıkla → workspace değiştir
- Uygulama ikonuna tıkla → o uygulamaya odaklan/ minimized'se geri yükle
- Uygulama ikonuna sağ tıkla → kapat menüsü
- Hover → tooltip (pencere adı)
- Sürükle → pencereyi o workspace'e taşı
- Sistem ikonuna tıkla → güç menüsü (Yeniden Başlat, Kapat, Oturum Kapat)

---

## 6. Alt Yatay Bar (linis-bar)

### 6.1 Konum ve Boyut
- Alt kenar, tam genişlik
- Yükseklik: 32px
- Sol kenar boşluğu: 48px (sol panel için)
- Arka plan: `%88 opacity, blur 12px, Tokyo Night BG_DARK`
- Kenar: Üstte 1px çizgi (ACCENT_BLUE, %40 opacity)

### 6.2 İçerik (soldan sağa)
```
┌────┬──────────────────────────┬────────┬──────┬──────┐
│ 🌸 │ Firefox    Thunar   ...  │  12:45 │ 🔊 🔋│ 📡 ⚙️│
└────┴──────────────────────────┴────────┴──────┴──────┘
 ▲    ▲                           ▲        ▲      ▲
 logo  Açık pencereler (taskbar)   saat    ses/   network/
                                                     ayarlar
```

### 6.3 Taskbar Davranışı
- Açık pencereler sırayla listelenir
- Aktif pencere: vurgulu arka plan + underline
- Hover: hafif highlight
- Tıkla → pencereye odaklan
- Orta tıkla → kapat
- Çalışmayan uygulama ikonu: soluk

### 6.4 Sistem Tepsisi (sağ taraf)
- **Saat:** `HH:MM` formatı, her 30 saniyede güncelle
- **Ses:** 🔊 ikonu, tıkla → pavucontrol aç
- **Pil:** 🔋/🔌 ikonu (laptop varsa göster)
- **Ağ:** 📡 ikonu, tıkla → nm-applet menüsü
- **Güç:** ⏻ ikonu, tıkla → güç menüsü

---

## 7. Uygulama Launcher (linis-launch)

### 7.1 Tetikleme
- `Super` tuşuna bas → launcher aç
- `Escape` → kapat
- `Enter` → seçili uygulamayı başlat
- Yazı yazarak filtreleme

### 7.2 Görünüm
```
┌─────────────────────────────────────┐
│ 🔍 Uygulama ara...                  │  ← Arama çubuğu
├─────────────────────────────────────┤
│ 📁 Thunar        Dosya Yöneticisi  │  ← Filtrelenmiş sonuçlar
│ 🌐 Firefox       Web Tarayıcı      │
│ 💬 Discord        Sohbet            │
│ ⚙️ Ayarlar       Sistem Ayarları    │
│ 🖥️ Terminal       xfce4-terminal    │
│ 📷 Ristretto     Resim Görüntüleyici│
│ 🎵 VLC           Medya Oynatıcı    │
└─────────────────────────────────────┘
```

### 7.3 Tasarım
- Arka plan: `%92 opacity, blur 20px, BG_MID`
- Kenar: 1px ACCENT_BLUE
- Yuvarlak köşe: 12px
- Gölge: 24px, %40 opacity
- Sonuçlar: Seçili = BG_LIGHT + sol tarafta mavi çizgi
- İkonlar: sol tarafta 32x32
- Animasyon: Açılış scale-up 0.2s, kapanış scale-down 0.15s

### 7.4 Kaynaklar
- `.desktop` dosyalarından uygulama listesi
- `.local/share/applications/` ve `/usr/share/applications/`
- Hotkey desteği: uygulama adı yazarak kısayol ata (örn: `ff` → Firefox)

---

## 8. Duvar Kağıdı

- `nitrogen` veya `feh` ile ayarlanır
- Varsayılan: `sakura-gradient.jpg` (Hiroki duvar kağıdı)
- Tüm workspace'ler için aynı duvar kağıdı
- Panel/bar üzerinde blur efekti uygulanır

---

## 9. Tema Dosyası (linis-theme.toml)

```toml
[colors]
bg_dark = "#1a1b26"
bg_mid = "#24283b"
bg_light = "#414868"
fg_primary = "#c0caf5"
fg_dim = "#565f89"
accent_blue = "#7aa2f7"
accent_purple = "#bb9af7"
accent_cyan = "#7dcfff"
accent_green = "#9ece6a"
accent_red = "#f7768e"
accent_orange = "#e0af68"
border_active = "#7aa2f7"
border_normal = "#24283b"

[compositor]
shadow_radius = 12
shadow_opacity = 0.3
blur_radius = 8
corner_radius = 10
inactive_opacity = 0.92
animation_duration_ms = 300

[panel_left]
width = 48
opacity = 0.88
blur = true
edge = "left"

[panel_bottom]
height = 32
opacity = 0.88
blur = true
edge = "bottom"

[gaps]
outer = 6
inner = 6

[keybinds]
# Super alone = launcher
# Super+T = toggle tiling
# Super+Q = close window
# Super+F = fullscreen
# Super+M = monocle
# Super+1-4 = switch workspace
# Super+Shift+1-4 = move window to workspace
# Super+H/L = resize master area
# Super+J/K = cycle windows
# Super+Space = toggle floating
# Super+Shift+R = restart linis
# Super+Shift+Q = quit linis
# Ctrl+Alt+T = terminal
# Ctrl+Alt+F = file manager
```

---

## 10. Dosya Yapısı

```
/usr/bin/
├── linis                    # Ana window manager binary
├── linis-panel              # Sol panel + alt bar (tek process)
├── linis-launch             # Uygulama launcher
├── linis-restart            # WM'i yeniden başlat
└── linis-session            # Oturum başlatıcı (xinit client)

/usr/share/linis/
├── themes/
│   └── tokyo-night.toml     # Varsayılan tema
├── keybinds.conf            # Kısayol atamaları
├── autostart                # Oturuş açılışında çalışan uygulamalar
└── logo.png                 # Sol panel logosu

/etc/xdg/linis/
├── config.toml              # Sistem genelinde config
└── autostart                # Sistem genelinde autostart

~/.config/linis/
├── config.toml              # Kullanıcı config'i
├── keybinds.conf            # Kullanıcı kısayolları
└── autostart                # Kullanıcı autostart
```

---

## 11. Oturum Başlatma Akışı

```
1. systemd → graphical.target
2. hiroki-dm.service → xinit /usr/bin/linis-session
3. linis-session:
   a. X sunucusunu başlat (xia)
   b. xhost +local: (yerel bağlantılara izin ver)
   c. su - kullanıcı -c "linis &"
   d. linis autostart'ı çalıştır:
      - nitrogen --restore &     (duvar kağıdı)
      - linis-panel &            (panel)
      - picom --config /dev/null (eğer compositor linis içinde değilse)
      - volumeicon &             (ses ikonu)
      - nm-applet &              (ağ)
      - xfce4-power-manager &    (pil yönetimi)
      - dunst &                  (bildirimler)
```

---

## 12. Installer Entegrasyonu

### Installer DE Seçenekleri
Artık `DES` dict'i şöyle olacak:
```python
DES = {
    "linis": {"name": "Linis", "ram": 1024, "desc": "Tokyo Night tarzı custom WM",
              "packages": ("linis",), "dm": "lightdm"},
    "openbox": {"name": "Openbox", ...},
    "lxqt": {"name": "LXQt", ...},
    "xfce": {"name": "XFCE", ...},
    "mate": {"name": "MATE", ...},
    "cinnamon": {"name": "Cinnamon", ...},
    "kde": {"name": "KDE Plasma", ...},
    "gnome": {"name": "GNOME", ...},
}
```

### Canlı Oturum
- Varsayılan live DE: `linis`
- `customize_airootfs.sh` → `SELECTED_DE=linis`
- `/etc/hiroki/selected-de` → `linis`

---

## 13. Build Sistemi

### Derleme (Makefile)
```makefile
CC = gcc
CFLAGS = -O2 -Wall -Wextra -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lX11 -lXrender -lXcomposite -lXdamage -lXfixes -lXext -lXrandr -lXinerama -lXft -lfontconfig -lfreetype -lpthread -lm

all: linis linis-panel linis-launch

linis: linis.c linis_compositor.c linis_config.c linis_keybind.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

linis-panel: linis_panel.c linis_config.c
	$(CC) $(CFLAGS) -o $@ $^ -lX11 -lXft -lfontconfig -lfreetype -lm

linis-launch: linis_launch.c
	$(CC) $(CFLAGS) -o $@ $^ -lX11 -lXft -lfontconfig -lfreetype -lm
```

### Paket (PKGBUILD)
```bash
pkgname=linis-wm
pkgver=1.0.0
pkgrel=1
pkgdesc="Linis Window Manager - Tokyo Night rice style X11 WM"
arch=('x86_64')
depends=('libx11' 'libxrender' 'libxcomposite' 'libxdamage' 'libxfixes'
         'libxext' 'libxrandr' 'libxinerama' 'libxft' 'fontconfig' 'freetype2')
source=()
build() {
    cd "$srcdir"
    make
}
package() {
    cd "$srcdir"
    make DESTDIR="$pkgdir" install
}
```

---

## 14. Performans Hedefleri

- **Idle CPU:** %0 (compositor idle'da frame çizmez)
- **Animasyon:** 60fps hedefi, vsync ile
- **Bellek:** <15MB resident
- **Başlama süresi:** <200ms (X bağlantısı + ilk frame)

---

## 15. NOTLAR (AI'a)

1. **Compositor XRender kullan** — GLX değil, daha az bağımlılık
2. **EWMH tam uyumlu ol** — panel, launcher, DE'ler çalışsın
3. **ICCCM uyumlu ol** — eski uygulamalar çalışsın
4. **Config TOML formatında** — Python tomllib ile okunur, C'de basit parser
5. **Panel tek process** — sol bar + alt bar tek binary içinde
6. **Görsel efektlere odaklan** — blur, gölge, yuvarlak köşe, animasyon
7. **Tokyo Night renklerini hardcoded yapma** — theme dosyasından oku
8. **Workspace 4 adet** — Super+1-4 ile geçiş
9. **Tiling layout basit tut** — master+stack, fancy grid lazım değil
10. **Autostart dosyası** — shell script olarak, bash ile çalıştırılır
