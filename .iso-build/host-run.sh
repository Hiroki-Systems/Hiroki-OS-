#!/usr/bin/env bash
# Hiroki OS ISO derlemesi - ana makine tarafi (ROOT ile calistirilir)
# Is: docker servisini baslatir, archlinux imajini ceker, derleme konteynerini arka planda baslatir.
set -euo pipefail

SELF_DIR="$(cd "$(dirname "$0")" && pwd)"
HOST_PROJ="$(cd "$SELF_DIR/.." && pwd)"   # hiroki-os/

echo "[host] Docker servisi kontrol ediliyor..."
systemctl is-active docker >/dev/null 2>&1 || systemctl start docker
for i in $(seq 1 15); do
    docker info >/dev/null 2>&1 && break
    sleep 1
done
docker info >/dev/null 2>&1 || { echo "[host] Docker baslatilamadi"; exit 1; }

# Kalicilik: derlenmis AUR paketleri + pacman onbellegi proje dizininde tutulur,
# boylece konteyner yeniden baslatilinca calamares yeniden derlenmez, indirmeler tekrarlanmaz.
mkdir -p "$HOST_PROJ/.iso-build/aur-repo" "$HOST_PROJ/.iso-build/pacman-cache"

if docker ps -a --format '{{.Names}}' | grep -q '^hiroki-iso-build$'; then
    echo "[host] Eski konteynerin AUR paketleri tasiniyor (varsa)..."
    docker cp "hiroki-iso-build:/localrepo/." "$HOST_PROJ/.iso-build/aur-repo/" 2>/dev/null || true
    ls -lh "$HOST_PROJ/.iso-build/aur-repo/" 2>/dev/null || true
    echo "[host] Eski konteyner siliniyor..."
    docker rm -f hiroki-iso-build >/dev/null 2>&1 || true
fi

echo "[host] archlinux:latest cekiliyor..."
docker pull archlinux:latest >/dev/null

echo "[host] Derleme konteyneri baslatiliyor (4 CPU / 6 GB, privileged)..."
docker run -d --name hiroki-iso-build \
    --cpus=4 --memory=6g --memory-swap=6g \
    --privileged \
    -v "$HOST_PROJ:/hiroki-os" \
    -v "$HOST_PROJ/.iso-build/pacman-cache:/var/cache/pacman/pkg" \
    archlinux:latest \
    bash /hiroki-os/.iso-build/entrypoint.sh

echo "[host] KONTEYNER BASLATILDI: hiroki-iso-build"
echo "[host] Log takibi: tail -f $HOST_PROJ/.iso-build/build.log"
echo "[host] Cikti: $HOST_PROJ/out/"
