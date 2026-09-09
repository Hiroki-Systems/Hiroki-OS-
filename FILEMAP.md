# 🗂️ Hiroki OS Dosya Haritası — Tam Dizin Yapısı

Bu belge projenin **gerçek, çalışır** tüm dosyalarını listeler. Her dosya kopyala-yapıştır kullanıma hazırdır.

```
hiroki-os/
├── profiledef.sh
├── packages.x86_64
├── pacman.conf
├── build.sh
├── README.md
├── BUILDING.md
├── INSTALL.md
├── CUSTOMIZATION.md
├── CONTRIBUTING.md
├── LICENSE
├── TESTING.md
├── FILEMAP.md (bu dosya)
│
├── efiboot/
│   ├── loader/loader.conf
│   └── loader/entries/hiroki.conf
├── syslinux/syslinux.cfg
├── grub/grub.cfg
│
└── airootfs/
    ├── etc/
    │   ├── hostname                     # hiroki
    │   ├── locale.conf                  # LANG=tr_TR.UTF-8
    │   ├── locale.gen                   # tr,en,ja,de,fr...
    │   ├── vconsole.conf                # KEYMAP=trq
    │   ├── os-release                   # Hiroki OS 1.0 Sakura
    │   ├── lsb-release
    │   ├── pacman.d/mirrorlist
    │   ├── sudoers.d/10-hiroki-live
    │   ├── lightdm/lightdm.conf
    │   ├── lightdm/lightdm-gtk-greeter.conf
    │   ├── skel/
    │   │   ├── .bashrc                  # Hiroki prompt + fastfetch
    │   │   ├── Pictures/wallpaper.jpg   # sakura-gradient
    │   │   └── .config/
    │   │       ├── autostart/hiroki-welcome.desktop
    │   │       ├── gtk-3.0/{settings.ini,gtk.css}
    │   │       ├── xfce4/...            # XFCE panel/xsettings
    │   │       ├── conky/hiroki-conky.conf
    │   │       ├── waybar/{config,style.css}
    │   │       ├── wofi/hiroki.css
    │   │       ├── hypr/hyprpaper.conf
    │   │       ├── mako/config
    │   │       ├── picom/picom.conf
    │   │       ├── dunst/dunstrc
    │   │       └── fastfetch/config.jsonc
    │   ├── hiroki/
    │   │   ├── branding/
    │   │   ├── de-selector/recommendations.json
    │   │   └── post-install/
    │   │       ├── hiroki-post-install.sh
    │   │       ├── apply-theme.sh
    │   │       ├── enable-services.sh
    │   │       └── setup-displaymanager.sh
    │   ├── calamares/
    │   │   ├── settings.conf            # show/exec sırası (hiroki-de-select dahil)
    │   │   ├── branding/hiroki/
    │   │   │   ├── branding.desc        # 900x600, Hiroki renkleri
    │   │   │   ├── show.qml             # 5 slayt
    │   │   │   ├── hiroki-logo.png
    │   │   │   ├── hiroki-icon.png
    │   │   │   ├── hiroki-welcome.png
    │   │   │   └── slide1..5.png
    │   │   └── modules/
    │   │       ├── welcome.conf         # 15GB, 512MB, internet check
    │   │       ├── locale.conf
    │   │       ├── keyboard.conf
    │   │       ├── partition.conf       # Btrfs @, @home, @snapshots, swap choices
    │   │       ├── users.conf           # /bin/bash, wheel vb.
    │   │       ├── displaymanager.conf  # lightdm/sddm/gdm/ly
    │   │       ├── packagechooser_hiroki.conf # 10 DE, paket listeleri
    │   │       ├── netinstall.conf      # Ek yazılım grupları
    │   │       ├── packages.conf
    │   │       ├── shell.conf           # post-install 4 script
    │   │       ├── fstab.conf
    │   │       ├── bootloader.conf      # GRUB
    │   │       ├── services-systemd.conf
    │   │       └── finished.conf
    │   └── systemd/system/
    │       ├── hiroki-live.service
    │       └── display-manager.service.d/
    ├── usr/
    │   ├── bin/
    │   │   ├── hiroki-hw-detect         # Bash - donanım algılama
    │   │   ├── hiroki-de-selector       # Wrapper → Python GTK
    │   │   ├── hiroki-welcome           # Wrapper → Python GTK
    │   │   ├── hiroki-theme-manager     # Python GTK
    │   │   ├── hiroki-update            # Python GTK + VTE
    │   │   ├── hiroki-snapshot          # Python GTK
    │   │   ├── hiroki-driver-manager    # Python GTK
    │   │   └── hiroki-live-setup        # Bash - live hazırlık
    │   ├── share/
    │   │   ├── hiroki/
    │   │   │   ├── de-selector/hiroki-de-selector.py  # Ana GUI (10 DE grid)
    │   │   │   └── wallpapers/5x.jpg + hiroki-logo.png
    │   │   ├── hiroki-welcome/hiroki-welcome.py       # Hoş geldiniz (live/ilk boot)
    │   │   ├── themes/Hiroki-Dark/{gtk-3.0/gtk.css,gtk-2.0/gtkrc,index.theme}
    │   │   ├── icons/Hiroki-Icons/index.theme
    │   │   ├── wallpapers/ (symlink kopya)
    │   │   ├── grub/themes/hiroki/theme.txt + background.png
    │   │   ├── sddm/themes/hiroki/{theme.conf,Main.qml}
    │   │   ├── plymouth/themes/hiroki/{hiroki.plymouth,hiroki.script}
    │   │   ├── hiroki/neofetch/config.conf
    │   │   ├── hiroki/ascii/hiroki.txt
    │   │   └── applications/hiroki-welcome.desktop
    │   └── lib/calamares/modules/ (Calamares yerel modüller)
    └── root/ (boş, archiso tarafından kullanılır)
```

### Kritik Dosya Amaçları

| Dosya | Amaç |
|---|---|
| `hiroki-hw-detect` | RAM/CPU/GPU/disk/virt algılar → `recommendations.json` mantığı → `/tmp/hiroki-hw-info.json` |
| `hiroki-de-selector.py` | GTK grid, sol donanım, sağ 10 DE, uyarı dialogu, `/tmp/hiroki-selected-de` yazar |
| `hiroki-welcome.py` | Live: DE Seç + Kurulum; Kurulu: Güncelle + Tema |
| `packagechooser_hiroki.conf` | Calamares instance `hiroki-de-select` → 10 DE paket listeleri |
| `apply-theme.sh` | Seçilen DE'ye göre Hiroki temayı `/etc/skel` ve kullanıcıya uygular |
| `setup-displaymanager.sh` | DE → DM eşleşmesi (KDE→sddm, GNOME→gdm, diğer→lightdm) |
| `profiledef.sh` | `HIROKI_OS` etiketi, `hiroki-os` ismi, file_permissions executable |
| `build.sh` | 17 adımlı ISO derleme, checksum, QEMU talimatı |

Tüm dosyalar POSIX/Bash/Python3/GTK3/Systemd/Calamares resmi dokümantasyonuyla uyumludur.
