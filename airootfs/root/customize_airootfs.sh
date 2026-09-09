#!/bin/bash
# Archiso hook: SquashFS olusturulmadan once calisir

# Live kullanici olustur
useradd -m -G wheel,video,audio,storage,network,users,lp,scanner,uucp -s /bin/bash live 2>/dev/null || true

# Sifre ayarla
passwd --delete live 2>/dev/null || true
echo "live:live" | chpasswd

# Home izinleri
chown -R live:live /home/live
chmod 700 /home/live

# Home dizinine tum skel dosyalarini kopyala
cp -a /etc/skel/. /home/live/ 2>/dev/null || true
# Tum alt dizinleri olustur
mkdir -p /home/live/{Documents,Downloads,Music,Pictures,Videos,Templates,Public} 2>/dev/null || true
# Wallpapers symlink'lerini kopyala (symlink olarak kalsin)
for f in /usr/share/hiroki/wallpapers/*; do
  ln -sf "$f" "/home/live/Pictures/$(basename "$f")" 2>/dev/null || true
done
for f in /usr/share/backgrounds/xfce/*; do
  ln -sf "$f" "/home/live/Pictures/$(basename "$f")" 2>/dev/null || true
done
# Thunar, xfconf, mimeapps, xfce4 config kopyala
cp -a /etc/skel/.config/Thunar /home/live/.config/ 2>/dev/null || true
cp -a /etc/skel/.config/xfce4 /home/live/.config/ 2>/dev/null || true
cp -a /etc/skel/.config/mimeapps.list /home/live/.config/ 2>/dev/null || true
cp -a /etc/skel/.config/gtk-3.0 /home/live/.config/ 2>/dev/null || true
cp -a /etc/skel/.config/xfce4/terminal /home/live/.config/xfce4/ 2>/dev/null || true
cp -a /etc/skel/.gtkrc-2.0 /home/live/ 2>/dev/null || true
cp -a /etc/skel/.config/user-dirs.dirs /home/live/.config/ 2>/dev/null || true
cp -a /etc/skel/.local /home/live/ 2>/dev/null || true
chmod 644 /home/live/.bash_profile 2>/dev/null || true
chmod 755 /home/live/.xinitrc 2>/dev/null || true
chown -R live:live /home/live

# /tmp izni
chmod 1777 /tmp

# D-Bus
dbus-uuidgen --ensure=/etc/machine-id 2>/dev/null || true

# Installer izni
chmod 755 /usr/bin/hiroki-installer 2>/dev/null || true

# glycin SVG loader bwrap sandbox live ortamda calismiyor
# Sadece sorunlu loader'lari ve icon temasini kaldir
rm -rf /usr/lib/glycin-loaders 2>/dev/null || true
pacman -Rns --noconfirm elementary-icon-theme 2>/dev/null || true
gdk-pixbuf-query-loaders --update-cache 2>/dev/null || true

# GTK3 varsayilan icon theme (SVG-crash olmasin)
mkdir -p /etc/gtk-3.0
cat > /etc/gtk-3.0/settings.ini <<'GTKEOF'
[Settings]
gtk-icon-theme-name=Adwaita
gtk-theme-name=Adwaita
GTKEOF

# glycin sandbox devre disi - tum oturumlarda
cat >> /home/live/.bash_profile <<'BASHEOF'
export GDK_DISABLE_SANDBOXED_LOADER=1
BASHEOF

# NetworkManager
systemctl enable NetworkManager.service 2>/dev/null || true
systemctl enable NetworkManager-wait-online.service 2>/dev/null || true
systemctl enable sshd.service 2>/dev/null || true

# DE paketleri ISO cache'inde hazır (962 paket, ~690MB)
# Kurulumda pacstrap -c ile bu cache'den hızlı kurulur

# --- Display Manager: getty autologin + .bash_profile (Wayland) ---
systemctl disable sddm.service 2>/dev/null || true
systemctl disable hiroki-dm.service 2>/dev/null || true
systemctl enable getty@tty1.service 2>/dev/null || true
systemctl set-default graphical.target 2>/dev/null || true
echo "kde" > /etc/hiroki/selected-de 2>/dev/null || true
# live kullanici .bash_profile'i
mkdir -p /home/live
cat > /home/live/.bash_profile <<'BPROFILE'
if [ -z "$WAYLAND_DISPLAY" ] && [ "$XDG_VTNR" = "1" ]; then
    export XDG_SESSION_TYPE=wayland
    export XDG_SESSION_DESKTOP=KDE
    export GDK_DISABLE_SANDBOXED_LOADER=1
    SESSION="kde"
    [ -f /etc/hiroki/selected-de ] && SESSION=$(cat /etc/hiroki/selected-de | tr -d '[:space:]')
    case "$SESSION" in
        kde|plasma)  exec startplasma-wayland ;;
        lxqt)        exec startlxqt ;;
        *)           exec startplasma-wayland ;;
    esac
fi
BPROFILE
chmod 644 /home/live/.bash_profile
chown 1000:1000 /home/live/.bash_profile 2>/dev/null || true

# --- Hiroki OS Branding: tum Arch yazilarini Hiroki yap ---
# os-release
rm -f /etc/os-release /usr/lib/os-release 2>/dev/null || true
cat > /etc/os-release << 'OSREL'
NAME="Hiroki OS"
PRETTY_NAME="Hiroki OS 1.0 Sakura"
ID=hiroki
ID_LIKE=arch
BUILD_ID=rolling
VERSION="1.0 Sakura"
VERSION_ID="1.0"
HOME_URL="https://hiroki.os"
BUG_REPORT_URL="https://hiroki.os/issues"
LOGO=linux
OSREL
# lsb-release
rm -f /etc/lsb-release 2>/dev/null || true
cat > /etc/lsb-release << 'LSB'
DISTRIB_ID=Hiroki
DISTRIB_RELEASE=1.0
DISTRIB_CODENAME=Sakura
DISTRIB_DESCRIPTION="Hiroki OS 1.0 Sakura"
LSB

# Local pacman repo ekle (kurulumda paketler internetten inmesin)
if [ -f /usr/share/hiroki/cache/hiroki-local.db ]; then
  cat >> /etc/pacman.conf << 'REPO'

[hiroki-local]
SigLevel = Optional TrustAll
Server = file:///usr/share/hiroki/cache
REPO
  echo "Local repo eklendi: /etc/pacman.conf"
fi

# Kernel + initramfs + GRUB kur (Docker post-install hook'lari calismiyor)
# vmlinuzu modules dizininden /boot'a kopyala
if [ -d /usr/lib/modules ]; then
  _vm=$(find /usr/lib/modules -name 'vmlinuz' -type f 2>/dev/null | head -1)
  if [ -n "$_vm" ]; then
    cp "$_vm" /boot/vmlinuz-linux
    echo "vmlinuz kopyalandi: $_vm"
  fi
fi
# preset dosyasini her zaman olustur (mevcut olsa bile uzerine yaz)
if [ -f /boot/vmlinuz-linux ]; then
  cat > /etc/mkinitcpio.d/linux.preset << PRESET
ALL_kver=/boot/vmlinuz-linux
PRESETS=('default' 'fallback')
default_image=/boot/initramfs-linux.img
fallback_image=/boot/initramfs-linux-fallback.img
fallback_options="-S autodetect"
PRESET
  echo "mkinitcpio preset yazildi"
fi
# initramfs olustur (proc/dev/sys mount gerekli)
mount -t proc proc /proc 2>/dev/null || true
mount -t sysfs sys /sys 2>/dev/null || true
mount --bind /dev /dev 2>/dev/null || true
mount --bind /dev/pts /dev/pts 2>/dev/null || true
mount --bind /dev/shm /dev/shm 2>/dev/null || true
echo "mount tamam, mkinitcpio baslatiliyor..."
mkinitcpio -P 2>&1 || true
echo "mkinitcpio tamamlandi"
umount /dev/shm /dev/pts /dev /sys /proc 2>/dev/null || true
# GRUB kur
if [ -f /boot/vmlinuz-linux ]; then
  mkdir -p /boot/grub
  grub-install --target=x86_64-efi --efi-directory=/boot --bootloader-id=HIROKI --recheck 2>&1 || true
  grub-mkconfig -o /boot/grub/grub.cfg 2>&1 || true
  echo "GRUB kuruldu"
fi

# KDE plasmoidleri ve ayarlari kur
echo "KDE ozellestirmeleri kuruluyor..."
mkdir -p /etc/skel/.local/share/plasma/plasmoids
cp -r /usr/share/plasma/plasmoids/luisbocanegra.panel.colorizer /etc/skel/.local/share/plasma/plasmoids/ 2>/dev/null || true
cp -r /usr/share/plasma/plasmoids/org.kde.latte.spacer /etc/skel/.local/share/plasma/plasmoids/ 2>/dev/null || true
# KDE config dosyalari skel'e kopyalandi (airootfs'ten gelir)
echo "KDE ozellestirmeleri tamamlandi"

# prison paketi Docker'da hook hatasi veriyor - IgnorePkg'a ekle
sed -i '/\[options\]/a IgnorePkg = prison' /etc/pacman.conf 2>/dev/null || true
