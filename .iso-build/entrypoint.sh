#!/usr/bin/env bash
# Hiroki OS ISO derlemesi - konteyner giriş noktası (root olarak çalışır)
set -euo pipefail

PROJECT=/hiroki-os
LOG="$PROJECT/.iso-build/build.log"
WORK=/work

mkdir -p "$(dirname "$LOG")"
: > "$LOG"
exec > >(tee -a "$LOG") 2>&1

echo "[Hiroki][$(date -Is)] Konteyner derlemesi basladi"

# Varsayilan mirrorlar (system update icin yeterli)
cat > /etc/pacman.d/mirrorlist <<'EOF'
Server = https://mirror.rackspace.com/archlinux/$repo/os/$arch
Server = https://geo.mirror.pkgbuild.com/$repo/os/$arch
Server = https://mirror.netcologne.de/archlinux/$repo/os/$arch
Server = https://mirrors.kernel.org/archlinux/$repo/os/$arch
EOF

echo "=== 1) Sistem guncelleniyor ==="
pacman -Syu --noconfirm

echo "=== 2) Derleme araclari + rate-mirrors kuruluyor ==="
pacman -S --needed --noconfirm \
    archiso squashfs-tools grub edk2-ovmf dosfstools mtools erofs-utils \
    libisoburn arch-install-scripts git sudo base-devel python curl \
    rate-mirrors

# Simdi hizli mirror bul
echo "=== 2b) Hizli mirrorlar taraniyor ==="
rate-mirrors --protocol https --save /etc/pacman.d/mirrorlist \
    --top-mirrors 10 \
    archlinux || true

echo "Secilen mirrorlar:"
cat /etc/pacman.d/mirrorlist

echo "=== 3) Profil /work altina kopyalaniyor ==="
rm -rf "$WORK"
mkdir -p "$WORK"
cp -a "$PROJECT/." "$WORK/profile"
rm -rf "$WORK/profile/out" "$WORK/profile/.iso-build"

echo "=== 4) mkarchiso calistiriliyor ==="
cd "$WORK/profile"
./build.sh

echo "=== 5) Ciktilari ana dizine kopyalama ==="
mkdir -p "$PROJECT/out"
cp -v "$WORK/profile/out/"* "$PROJECT/out/" 2>/dev/null || true
ls -lh "$PROJECT/out"

echo "[Hiroki][$(date -Is)] Derleme TAMAMLANDI"
