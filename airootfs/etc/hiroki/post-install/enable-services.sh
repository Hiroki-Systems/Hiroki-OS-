#!/usr/bin/env bash
# Hiroki OS - Servisleri Etkinleştir
set -euo pipefail
DE="${1:-xfce}"
echo "[Hiroki Servisler] DE: $DE"

# Temel servisler
systemctl enable NetworkManager.service 2>/dev/null || true
systemctl enable NetworkManager-dispatcher.service 2>/dev/null || true
systemctl enable bluetooth.service 2>/dev/null || true
systemctl enable cups.service 2>/dev/null || true
systemctl enable cups.socket 2>/dev/null || true
systemctl enable ufw.service 2>/dev/null || systemctl enable firewalld.service 2>/dev/null || true
systemctl enable fstrim.timer 2>/dev/null || true
systemctl enable reflector.timer 2>/dev/null || true

# Ses
systemctl --global enable pipewire.service 2>/dev/null || true
systemctl --global enable pipewire-pulse.service 2>/dev/null || true
systemctl --global enable wireplumber.service 2>/dev/null || true

# Zaman senkron
systemctl enable systemd-timesyncd.service 2>/dev/null || true

# Btrfs ise snapper/timeshift timer
if findmnt -n -o FSTYPE / | grep -q btrfs; then
    systemctl enable snapper-timeline.timer 2>/dev/null || true
    systemctl enable snapper-cleanup.timer 2>/dev/null || true
    systemctl enable grub-btrfs.path 2>/dev/null || true
fi

# Avahi (ağ keşfi)
systemctl enable avahi-daemon.service 2>/dev/null || true

echo "[Hiroki Servisler] Tamamlandı"
