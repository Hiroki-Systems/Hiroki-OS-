#!/usr/bin/env bash
# Hiroki OS - Ana Post-Install Script
# Calamares shell modülü tarafından chroot içinde çalıştırılır
# Seçilen DE'ye göre tema, DM ve servisleri yapılandırır
set -euo pipefail

LOG="/var/log/hiroki-post-install.log"
exec > >(tee -a "$LOG") 2>&1
echo "[Hiroki Post-Install] Başlıyor... $(date -Iseconds)"

# Seçilen DE'yi bul
SELECTED_DE=""
if [[ -f /tmp/hiroki-selected-de ]]; then
    SELECTED_DE=$(cat /tmp/hiroki-selected-de | tr -d '[:space:]')
elif [[ -f /tmp/hiroki-calamares-selection.json ]]; then
    SELECTED_DE=$(python3 -c "import json; print(json.load(open('/tmp/hiroki-calamares-selection.json')).get('selected',''))" 2>/dev/null || echo "")
fi

# Fallback: paket listesine bakarak tahmin et (eğer calamares netinstall kullanıldıysa)
if [[ -z "$SELECTED_DE" ]]; then
    echo "[Uyarı] Seçilen DE bulunamadı, varsayılan xfce kullanılacak"
    SELECTED_DE="xfce"
fi

echo "[Bilgi] Seçilen DE: $SELECTED_DE"
echo "$SELECTED_DE" > /etc/hiroki/selected-de 2>/dev/null || mkdir -p /etc/hiroki && echo "$SELECTED_DE" > /etc/hiroki/selected-de

# Çağrılacak alt scriptler
for script in /etc/hiroki/post-install/apply-theme.sh /etc/hiroki/post-install/enable-services.sh /etc/hiroki/post-install/setup-displaymanager.sh; do
    if [[ -x "$script" ]]; then
        echo "[Çalıştır] $script $SELECTED_DE"
        "$script" "$SELECTED_DE" || echo "[Hata] $script başarısız, devam ediliyor"
    fi
done

# Ek yapılandırmalar

# Varsayılan uygulamalar
if command -v xdg-mime >/dev/null 2>&1; then
    xdg-mime default hiroki-browser.desktop text/html 2>/dev/null || true
    xdg-mime default hiroki-browser.desktop x-scheme-handler/http 2>/dev/null || true
    xdg-mime default hiroki-browser.desktop x-scheme-handler/https 2>/dev/null || true
fi

# GRUB tema
if [[ -d /usr/share/grub/themes/hiroki ]]; then
    if grep -q "GRUB_THEME" /etc/default/grub 2>/dev/null; then
        sed -i 's|^GRUB_THEME=.*|GRUB_THEME="/usr/share/grub/themes/hiroki/theme.txt"|' /etc/default/grub
    else
        echo 'GRUB_THEME="/usr/share/grub/themes/hiroki/theme.txt"' >> /etc/default/grub
    fi
    # GRUB arkaplan ve timeout
    sed -i 's/^GRUB_TIMEOUT=.*/GRUB_TIMEOUT=10/' /etc/default/grub 2>/dev/null || echo 'GRUB_TIMEOUT=10' >> /etc/default/grub
    # Hiroki için os-prober aktif
    if ! grep -q "GRUB_DISABLE_OS_PROBER" /etc/default/grub; then
        echo 'GRUB_DISABLE_OS_PROBER=false' >> /etc/default/grub
    fi
fi

# Hiroki welcome ilk açılış ayarı - kullanıcı skel'e zaten eklendi, ama var olan kullanıcı için de
if id hiroki >/dev/null 2>&1; then
    sudo -u hiroki mkdir -p /home/hiroki/.config/autostart 2>/dev/null || true
fi
# /etc/skel için zaten var, yeni kullanıcılar için otomatik

# Fastfetch / neofetch config
if [[ -f /usr/share/hiroki/neofetch/config.conf ]]; then
    mkdir -p /etc/skel/.config/neofetch 2>/dev/null || true
    cp /usr/share/hiroki/neofetch/config.conf /etc/skel/.config/neofetch/config.conf 2>/dev/null || true
fi

# Plymouth tema (varsa)
if command -v plymouth-set-default-theme >/dev/null 2>&1 && [[ -d /usr/share/plymouth/themes/hiroki ]]; then
    plymouth-set-default-theme -R hiroki 2>/dev/null || echo "[Uyarı] plymouth tema ayarlanamadı"
fi

# Yay AUR helper'i kur (eğer seçildiyse veya varsayılan)
if ! command -v yay >/dev/null 2>&1; then
    echo "[Bilgi] yay kuruluyor..."
    # Live ortamda değilsek, kullanıcı yay'ı seçmişse Calamares zaten kurmuş olabilir, kontrol et
    if command -v pacman >/dev/null 2>&1; then
        # AUR helper için base-devel zaten var
        # Kullanıcı hiroki için yay kurulumu (makepkg gerekiyor)
        # Basit: eğer internet varsa yay-bin'i dene (ama offline ise atla)
        pacman -S --noconfirm --needed git base-devel 2>/dev/null || true
    fi
fi

# mkinitcpio: archiso hook'larini temizle + autodetect ekle (live ISO icindi, kurulum sistemi icin gerekmez)
if [[ -f /etc/mkinitcpio.conf ]]; then
    if grep -q 'archiso' /etc/mkinitcpio.conf; then
        sed -i 's/ archiso_loop_mnt//g; s/ archiso_pxe_common//g; s/ archiso//g' /etc/mkinitcpio.conf
        # autodetect hook'unu geri ekle (kurulum sistemi icin optimize)
        sed -i 's/modconf kms/modconf autodetect kms/' /etc/mkinitcpio.conf
        echo "[Hiroki Post-Install] mkinitcpio.conf guncellendi: archiso temizlendi, autodetect eklendi"
        mkinitcpio -P 2>/dev/null || echo "[Uyari] mkinitcpio -P basarisiz"
    fi
fi

# İlk boot flag temizle
mkdir -p /var/lib/hiroki
rm -f /var/lib/hiroki/first-boot-done 2>/dev/null || true

# Hostname zaten ayarlı, ama os-release kontrol
if ! grep -q "Hiroki OS" /etc/os-release 2>/dev/null; then
    cat > /etc/os-release <<'EOS'
NAME="Hiroki OS"
PRETTY_NAME="Hiroki OS 1.0 (Sakura)"
ID=hiroki
ID_LIKE=arch
BUILD_ID=rolling
VERSION="1.0 Sakura"
VERSION_ID="1.0"
VERSION_CODENAME=sakura
HOME_URL="https://hiroki-os.org"
DOCUMENTATION_URL="https://wiki.hiroki-os.org"
SUPPORT_URL="https://hiroki-os.org/support"
BUG_REPORT_URL="https://github.com/hiroki-os/hiroki-os/issues"
LOGO=linux
ANSI_COLOR="0;35"
EOS
fi

echo "[Hiroki Post-Install] Tamamlandı. Seçilen DE: $SELECTED_DE"
