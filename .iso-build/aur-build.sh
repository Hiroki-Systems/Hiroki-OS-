#!/usr/bin/env bash
# Hiroki OS ISO için AUR'dan derlenen paketler.
# Root OLMAYAN (builder) kullanıcı olarak çalıştırılır: runuser -u builder -- bash aur-build.sh /localrepo
set -euo pipefail

REPO="$1"
export HOME=/home/builder
cd /home/builder/aur

# Calamares ve bağımlılıkları artık resmi Arch depolarında yok (yalnız AUR).
# neofetch da AUR'a taşındı; fastfetch yanında opsiyonel olarak ekliyoruz.
AUR_PKGS=(ckbcomp mkinitcpio-openswap)

for p in "${AUR_PKGS[@]}"; do
    if ls "$REPO"/"$p"-*.pkg.tar.zst >/dev/null 2>&1; then
        echo "[AUR] $p zaten derlenmis, atlaniyor"
        continue
    fi
    echo "[AUR] $p indiriliyor..."
    curl -fsSLo "$p.tar.gz" "https://aur.archlinux.org/cgit/aur.git/snapshot/$p.tar.gz"
    rm -rf "$p"
    mkdir "$p"
    tar -xzf "$p.tar.gz" -C "$p" --strip-components=1
    cd "$p"
    echo "[AUR] $p derleniyor (makepkg)..."
    # --skippgpcheck: bazi AUR PKGBUILD'larinda git kaynagi icin GPG anahtari dogrulanamiyor
    makepkg -s --noconfirm -f --skippgpcheck
    cp ./*.pkg.tar.zst "$REPO"/
    cd ..
done

echo "[AUR] Tum paketler derlendi -> $REPO"
ls -lh "$REPO"
