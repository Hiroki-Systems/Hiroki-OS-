# 🎨 Hiroki OS Özelleştirme Rehberi

Sakura temasını, masaüstü ortamını ve kendi temanı nasıl oluşturacağını anlatır.

---

## 1. Renk Paleti

| İsim | Kod | Kullanım |
|---|---|---|
| Koyu mor (primary) | `#2D1B69` | Başlıklar, panel, vurgu |
| Canlı pembe (secondary) | `#E91E8C` | Buton, seçili, link |
| Turkuaz (tertiary) | `#00D4AA` | İkincil vurgu, badge |
| Koyu arkaplan | `#0D0D1A` | Pencere arkaplanı |
| Açık koyu | `#1A1A2E` | Kartlar, menüler |
| Metin primary | `#EAEAEA` | Ana metin |
| Metin secondary | `#A0A0B8` | Açıklama |

---

## 2. Tema Yöneticisi (GUI)

```bash
hiroki-theme-manager
```

- **Tema:** `Hiroki-Dark` (varsayılan) / `Hiroki-Light`
- **Vurgu rengi:** Pembe / Mor / Turkuaz (`~/.config/hiroki/accent.conf`)
- **Duvar kağıdı:** 5 seçenek → anında `feh`/`gsettings`/`xfconf` ile uygulanır

Manüel:

```bash
# GTK
gsettings set org.gnome.desktop.interface gtk-theme 'Hiroki-Dark'
gsettings set org.gnome.desktop.interface icon-theme 'Hiroki-Icons'
xfconf-query -c xsettings -p /Net/ThemeName -s Hiroki-Dark

# Duvar kağıdı (XFCE)
xfconf-query -c xfce4-desktop -p /backdrop/screen0/monitor0/workspace0/last-image -s /usr/share/hiroki/wallpapers/minimal-mountain.jpg
# GNOME
gsettings set org.gnome.desktop.background picture-uri file:///usr/share/hiroki/wallpapers/minimal-mountain.jpg
# i3/Hyprland
feh --bg-scale /usr/share/hiroki/wallpapers/space-theme.jpg
# Hyprland waybar vs.
```

---

## 3. GTK Teması

Konum: `/usr/share/themes/Hiroki-Dark/`

- `gtk-3.0/gtk.css` — Arc Dark tabanlı, Hiroki renkleri
- `gtk-2.0/gtkrc`

Kendi varyantını oluştur:

```bash
sudo cp -r /usr/share/themes/Hiroki-Dark /usr/share/themes/Hiroki-My
sudo nano /usr/share/themes/Hiroki-My/gtk-3.0/gtk.css
# @define-color hiroki_pink #E91E8C; satırını değiştir
gsettings set org.gnome.desktop.interface gtk-theme 'Hiroki-My'
```

---

## 4. İkon Teması

Konum: `/usr/share/icons/Hiroki-Icons/index.theme` — Papirus Dark miras, mor klasörler

Kendi ikonunu ekle:

```bash
mkdir -p ~/.local/share/icons/Hiroki-My/48x48/apps
cp my-icon.png ~/.local/share/icons/Hiroki-My/48x48/apps/
gtk-update-icon-cache ~/.local/share/icons/Hiroki-My
gsettings set org.gnome.desktop.interface icon-theme 'Hiroki-My'
```

---

## 5. Masaüstü Ortamı Değiştirme

### Kurulum Sonrası Yeni DE Ekle

```bash
# Örn. KDE ekle (XFCE kurulu iken)
sudo pacman -S plasma-meta kde-applications konsole dolphin kate sddm
sudo systemctl enable sddm --force
# GDM/LightDM çakışırsa disable et: sudo systemctl disable lightdm
```

### Varsayılan DE'yi Değiştir (Display Manager)

LightDM:
```bash
sudo nano /etc/lightdm/lightdm.conf
# user-session=xfce  →  plasma / gnome / budgie-desktop / lxqt / i3
```

SDDM (KDE):
```bash
sudo nano /etc/sddm.conf.d/hiroki.conf
# Session=xfce.desktop → plasma.desktop
```

### Hiroki DE Seçiciyi Yeniden Çalıştır

```bash
hiroki-de-selector
# Donanımı yeniden tarar, 10 DE arasında seçim, /tmp/hiroki-selected-de yazar
# Calamares kurulumunda bu seçim paketleri belirler; kurulu sistemde manuel pacman gerekir
```

---

## 6. Her DE İçin Özelleştirme İpuçları

### XFCE
- Panel: Sağ tık → Panel Tercihleri → Yarı saydam koyu (`#1A1A2E` 85%)
- Whisker Menü → Hiroki logosu: `/usr/share/icons/hiroki/hiroki-icon.png`
- Conky: `~/.config/conky/hiroki-conky.conf` → `conky -c ~/.config/conky/hiroki-conky.conf &`
- Thunar koyu tema zaten `settings.ini` ile gelir

### KDE Plasma
- Sistem Ayarları → Görünüm → Global Tema: Hiroki Dark
- Renk şeması: `/usr/share/color-schemes/HirokiDark.colors` (kopyala)
- SDDM: `/usr/share/sddm/themes/hiroki/`

### GNOME
- `gnome-tweaks` → Görünüm → Hiroki-Dark, Hiroki-Icons
- Eklentiler: Dash to Dock, AppIndicator, User Themes, Blur my Shell
- `dconf dump / > backup.dconf` ile yedek

### i3wm
- Config: `~/.config/i3/config` (Hiroki renkleri, gaps, picom, rofi)
- `rofi -show drun -theme /usr/share/rofi/themes/hiroki.rasi`
- `picom --experimental-backends &` (compositor)
- Kısayol rehberi: `Mod+F1` → `/usr/share/hiroki/i3-shortcuts.txt`

### Hyprland
- `~/.config/hypr/hyprland.conf` (mor/pembe border, rounding 12, blur)
- `waybar` → `~/.config/waybar/config` + `style.css` (Hiroki renkleri)
- `wofi` → `~/.config/wofi/hiroki.css`

---

## 7. GRUB Teması

Konum: `/usr/share/grub/themes/hiroki/theme.txt`

Değiştir:
```bash
sudo nano /etc/default/grub
# GRUB_THEME="/usr/share/grub/themes/hiroki/theme.txt"
sudo grub-mkconfig -o /boot/grub/grub.cfg
```

Kendi arkaplanın:
```bash
sudo cp ~/resim.png /usr/share/grub/themes/hiroki/background.png
```

---

## 8. Plymouth (Açılış Animasyonu)

```bash
sudo plymouth-set-default-theme -R hiroki
# Test: sudo plymouthd; sudo plymouth --show-splash; sleep 5; sudo plymouth quit
```

Tema dosyaları: `/usr/share/plymouth/themes/hiroki/hiroki.script`

---

## 9. Neofetch / Fastfetch

```bash
fastfetch --logo /usr/share/hiroki/ascii/hiroki.txt
neofetch --ascii_distro hiroki
```

Yapılandırma:
- `~/.config/neofetch/config.conf` → `/usr/share/hiroki/neofetch/config.conf` kopyalanır
- `~/.config/fastfetch/config.jsonc`

---

## 10. Kendi Temanı Oluştur ve Paketle

1. `hiroki-theme-my` klasörü oluştur
2. GTK, ikon, duvar kağıdı ekle
3. `PKGBUILD` yaz (AUR örneği):
```bash
pkgname=hiroki-theme-my
pkgver=1.0
pkgrel=1
arch=('any')
source=("my-theme.tar.gz")
package() { cp -r usr "$pkgdir"/ }
```
4. `makepkg -si`

Topluluğa gönder: https://github.com/hiroki-os/hiroki-os/issues

---

## 11. Dosya Haritası

```
airootfs/usr/share/themes/Hiroki-Dark/      # GTK
airootfs/usr/share/icons/Hiroki-Icons/      # İkon
airootfs/usr/share/hiroki/wallpapers/5x.jpg # Duvar kağıtları
airootfs/usr/share/grub/themes/hiroki/      # GRUB
airootfs/usr/share/plymouth/themes/hiroki/  # Plymouth
airootfs/etc/skel/.config/gtk-3.0/settings.ini # Varsayılan GTK
airootfs/etc/skel/.bashrc                   # Hiroki prompt + fastfetch
```

İyi özelleştirmeler! 🌸
