#!/usr/bin/env bash
# Hiroki OS - Display Manager Kurulumu
# Seçilen DE'den bağımsız olarak Hiroki'nin kendi giriş ekranı (hiroki-dm)
# aktive edilir. Geleneksel DM'ler (lightdm/sddm/gdm) devre dışı bırakılır ki
# grafik hedefiyle çakışmasınlar.
set -euo pipefail
DE="${1:-xfce}"
echo "[Hiroki DM] DE: $DE için hiroki-dm etkinleştiriliyor"

# --- Geleneksel DM'leri kapat (çakışmayı önle) ---
for dm in lightdm sddm gdm ly lxdm; do
    systemctl disable "$dm.service" 2>/dev/null || true
done

# --- Kurulu sistemin kullanıcısını bul (uid>=1000, çalışan kabuk) ---
INSTALLED_USER="$(getent passwd | awk -F: '($3>=1000 && $3<65534 && $7!~/\/false$/ && $7!~/\/sbin\/nologin$/){print $1}' | head -n1 || true)"
[[ -z "$INSTALLED_USER" ]] && INSTALLED_USER="hiroki"
echo "[Hiroki DM] Kullanıcı: $INSTALLED_USER"

# --- Oturumu DE'ye göre seç ---
SESSION="$DE"
case "$DE" in
    i3wm) SESSION="i3wm" ;;
    xfce|xfce4) SESSION="xfce" ;;
    kde) SESSION="kde" ;;
    gnome) SESSION="gnome" ;;
    mate) SESSION="mate" ;;
    cinnamon) SESSION="cinnamon" ;;
    budgie) SESSION="budgie" ;;
    lxqt) SESSION="lxqt" ;;
    openbox) SESSION="openbox" ;;
    hyprland) SESSION="hyprland" ;;
esac

# --- hiroki-dm.conf yaz ---
# Varsayılan: karşılama ekranı gösterilir (AUTO_LOGIN=no).
# Otomatik giriş istiyorsan aşağıdaki satırı "yes" yap.
mkdir -p /etc/hiroki 2>/dev/null || true
cat > /etc/hiroki/hiroki-dm.conf <<EOF
# Hiroki Display Manager (hiroki-dm) - kurulu sistem
# AUTO_LOGIN=yes -> açılışta $INSTALLED_USER otomatik giriş yapar
AUTO_LOGIN=no
DEFAULT_USER=$INSTALLED_USER
SESSION=$SESSION
DISPLAY_NUM=:1
VT=1
KILL_TIMEOUT=40
EOF

# Seçilen masaüstünü kaydet
echo "$SESSION" > /etc/hiroki/selected-de 2>/dev/null || true

# --- hiroki-dm servisini etkinleştir ---
systemctl enable hiroki-dm.service 2>/dev/null || {
    echo "[Uyarı] hiroki-dm.service etkinleştirilemedi"
}

# Grafik hedefi aç
systemctl set-default graphical.target 2>/dev/null || true

echo "[Hiroki DM] Tamamlandı (hiroki-dm, oturum: $SESSION)"
