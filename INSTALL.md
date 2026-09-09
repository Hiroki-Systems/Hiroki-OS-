# 💿 Hiroki OS Kurulum Rehberi — Sakura 1.0

Bu belge BIOS ve UEFI, disk bölümleme, dual-boot ve kurulum sonrası adımları kapsar.

---

## 1. Sistem Gereksinimleri

| Bileşen | Minimum | Önerilen |
|---|---|---|
| RAM | 512 MB (i3wm/Openbox) | 4 GB+ (KDE/GNOME rahat) |
| Disk | 15 GB (Calamares kontrol eder) | 30 GB+ (Btrfs snapshot için 40 GB) |
| CPU | 1 GHz, 1 çekirdek | 2 GHz, 2+ çekirdek |
| GPU | VESA uyumlu | Mesa / Vulkan destekli (AMD/Intel/NVIDIA) |
| Ortam | BIOS veya UEFI | UEFI + Secure Boot kapalı |

> `hiroki-hw-detect` kurulum öncesi donanımınızı analiz eder ve en uygun DE'yi önerir ama seçim serbesttir.

---

## 2. ISO'yu Hazırla

```bash
# İndir (yakında)
wget https://github.com/hiroki-os/hiroki-os/releases/download/v1.0/hiroki-os-1.0-x86_64.iso
sha256sum -c hiroki-os-*.iso.sha256

# USB'ye yaz (Linux)
lsblk
sudo dd if=hiroki-os-*.iso of=/dev/sdX bs=4M status=progress oflag=sync
# Windows: Rufus (DD modu), Etcher, Ventoy
# macOS: Etcher veya dd
```

Secure Boot: Hiroki OS Secure Boot imzalı değildir; BIOS'ta **Secure Boot'u kapatın** veya `shim` kullanın.

---

## 3. Boot Menüsü

ISO açıldığında GRUB/slideshow:

- **Hiroki OS'u Dene** — XFCE live, otomatik giriş (`hiroki` / şifre yok)
- **Hiroki OS'u Kur** — Doğrudan Calamares
- **Donanım Bilgisi** / **Bellek Testi**
- BIOS: Syslinux menüsü aynı seçenekleri sunar

Live masaüstünde:

- **Hiroki Hoş Geldiniz** otomatik açılır → **Masaüstü Ortamı Seç** ile öneriyi gör
- **Calamares** kısayolu (masaüstü/dock)

İnternet: Sağ üst ağ simgesi → Wi-Fi/Ethernet. Kurulum öncesi bağlanmanız önerilir (paketler için).

---

## 4. Calamares Adımları

1. **Welcome** — Dil seç (Türkçe varsayılan), internet/disk/RAM kontrolü
2. **Locale** — Bölge `Europe/Istanbul`, saat dilimi, `tr_TR.UTF-8`
3. **Keyboard** — `tr` (Q), model `pc105`
4. **Partition** — Bkz. aşağı
5. **Hiroki DE Select** — Akıllı öneri + 10 seçenek (XFCE, KDE, GNOME, Cinnamon, MATE, Budgie, LXQt, i3wm, Openbox, Hyprland)
   - Uyarı: 512 MB + KDE/GNOME seçerseniz “Ağır olabilir, devam edilsin mi?” sorusu gelir; onaylarsanız kurulur
6. **Netinstall** — Firefox, VLC, LibreOffice, Timeshift vb. ek yazılımlar
7. **Users** — Kullanıcı, şifre, hostname, autologin, root şifresi opsiyonel; varsayılan shell `/bin/bash`; gruplar `wheel,network,video,audio,storage`
8. **Summary** — Özet
9. **Install** → **Finished** → Yeniden başlat

---

## 5. Disk Bölümleme

### Otomatik (Önerilen - Yeni Başlayan)

Calamares → **Diski Sil** →

- **ext4:** Basit, stabil
- **btrfs:** Önerilen (snapshot desteği). Seçerseniz otomatik subvolume:
  ```
  @         → /
  @home     → /home
  @snapshots → /.snapshots
  @var_log  → /var/log
  @cache    → /var/cache
  ```
  + `compress=zstd:1,ssd,noatime`

Swap: **none / small (512 MB swap file) / suspend (RAM kadar) / file (özel)**

### Manuel

- **UEFI:** En az 300 MB FAT32 `/boot/efi` (esp, boot flag)
- **BIOS:** 1 MB `bios_grub` veya `/boot`
- `/` en az 15 GB, `ext4`/`btrfs`/`xfs`/`f2fs`
- EFI + `/` + `swap` + `/home` ayırabilirsiniz

### Dual Boot (Windows ile)

1. Windows'ta disk küçült (Disk Management → Shrink)
2. Hiroki kurulumunda **Yan yana kur** veya **Manuel** → boş alanı kullan
3. GRUB `os-prober` Windows'u otomatik bulur (`GRUB_DISABLE_OS_PROBER=false`)
4. BIOS boot sırası: Hiroki/Arch ilk

> UEFI + Windows BitLocker varsa BitLocker'ı duraklatın.

### Şifreli (LUKS)

Calamares → **Partition** → **Encrypt** → parola gir. `luksbootkeyfile` modülü anahtar dosyayı yönetir.

---

## 6. BIOS vs UEFI Farkları

| Özellik | BIOS (Legacy) | UEFI |
|---|---|---|
| Partition tablosu | MBR | GPT (önerilen) |
| Bootloader | GRUB MBR | GRUB ESP (`/boot/efi`) |
| Secure Boot | Yok | Kapat önerilir |
| Calamares modu | `bios.syslinux.mbr` | `uefi-x64.grub.esp` |
| Komut | `grub-install --target=i386-pc /dev/sda` | `grub-install --target=x86_64-efi --efi-directory=/boot/efi` |

Her ikisi de Hiroki profilinde tanımlı (`profiledef.sh` → `bootmodes`).

---

## 7. Kurulum Sonrası İlk Açılış

1. GRUB → Hiroki temalı menü (10 sn)
2. Plymouth sakura animasyonu (varsa)
3. Display Manager:
   - XFCE/Cinnamon/MATE/Budgie/LXQt/Openbox → **LightDM** (Hiroki GTK greeter)
   - KDE → **SDDM** (Hiroki teması)
   - GNOME → **GDM**
   - i3/Hyprland → LightDM veya `ly`
4. Giriş → Seçtiğiniz DE Hiroki temasıyla açılır
5. **Hiroki Hoş Geldiniz** ilk açılışta → Güncelle, tema, yedek rehberi
6. `sudo pacman -Syu` ve `yay` ile sistemi güncel tut

Servisler otomatik etkin: `NetworkManager`, `bluetooth`, `cups`, `ufw`, `fstrim.timer`, `pipewire` + `wireplumber`

---

## 8. Kurulum Sonrası Yapılacaklar

```bash
# Güncelle
sudo pacman -Syu
# veya grafik: hiroki-update

# AUR helper (yay)
sudo pacman -S --needed git base-devel
git clone https://aur.archlinux.org/yay.git && cd yay && makepkg -si

# Sürücüler (NVIDIA vb.)
hiroki-driver-manager

# Yedek
sudo timeshift --create --comments "ilk kurulum"

# Tema
hiroki-theme-manager
```

---

## 9. Sorun Giderme

- **Wi-Fi yok:** `nmtui` veya `iwctl` ile bağlan; `lspci -k` ile sürücü kontrol et; `hiroki-driver-manager`
- **Siyah ekran (NVIDIA):** GRUB'da `e` → `nomodeset` ekle → açılış sonrası `sudo pacman -S nvidia` ve `mkinitcpio -P`
- **GRUB kayboldu (dual-boot):** Live ile aç → `sudo mount /dev/sdXn /mnt` + `sudo arch-chroot /mnt grub-install ... && grub-mkconfig -o /boot/grub/grub.cfg`
- **Btrfs snapshot geri al:** `sudo snapper list` veya `timeshift --restore`
- **Şifre unutuldu:** GRUB → `e` → `init=/bin/bash` → `passwd hiroki`

---

## 10. Kaldırma

Canlı USB ile aç → GParted → Hiroki partition'larını sil → Windows `bootrec /fixmbr` (BIOS) veya EFI'den Hiroki girdisini sil.

---

Keyifli Hiroki! 🌸 https://hiroki-os.org/support
