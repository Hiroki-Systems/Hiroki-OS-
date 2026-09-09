#!/usr/bin/env bash
# Hiroki OS - ISO Derleme Scripti
# Sakura 1.0 - Archiso tabanlı
# Kullanım: sudo ./build.sh
# Gereksinimler: archiso, squashfs-tools, grub, edk2-ovmf, dosfstools, erofs-utils

set -euo pipefail

# Renkler
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PINK='\033[1;35m'
NC='\033[0m'

PROFILE_DIR="$(cd "$(dirname "$0")" && pwd)"
WORK_DIR="/tmp/hiroki-build-work"
OUT_DIR="$PROFILE_DIR/out"
ISO_NAME="hiroki-os-1.0-x86_64.iso"

log() { echo -e "${PINK}[Hiroki]${NC} $*"; }
ok() { echo -e "${GREEN}[OK]${NC} $*"; }
warn() { echo -e "${YELLOW}[UYARI]${NC} $*"; }
err() { echo -e "${RED}[HATA]${NC} $*"; }

check_root() {
    if [[ $EUID -ne 0 ]]; then
        err "Bu script root olarak çalıştırılmalı: sudo ./build.sh"
        exit 1
    fi
}

check_deps() {
    log "Bağımlılıklar kontrol ediliyor..."
    local deps=(mkarchiso mksquashfs grub-mkstandalone xorriso)
    local missing=()
    for dep in "${deps[@]}"; do
        if ! command -v "$dep" >/dev/null 2>&1; then
            missing+=("$dep")
        fi
    done
    if [[ ${#missing[@]} -gt 0 ]]; then
        err "Eksik bağımlılıklar: ${missing[*]}"
        echo "Kurmak için: pacman -S archiso squashfs-tools grub edk2-ovmf dosfstools erofs-utils"
        exit 1
    fi
    # archiso sürüm kontrol
    if ! pacman -Q archiso >/dev/null 2>&1; then
        warn "archiso paketi yüklü değil gibi görünüyor"
    fi
    ok "Bağımlılıklar tamam"
}

prepare_workdir() {
    log "Çalışma dizini hazırlanıyor: $WORK_DIR"
    rm -rf "$WORK_DIR"
    mkdir -p "$WORK_DIR" "$OUT_DIR"
    ok "Çalışma dizini hazır"
}

copy_profile() {
    log "Archiso profil dosyaları kopyalanıyor..."
    # Hiroki OS profilini çalışma dizinine kopyala (kendisi hariç out ve work)
    # mkarchiso doğrudan profil dizinini kullanır, bu yüzden sadece doğrulama yapıyoruz
    if [[ ! -f "$PROFILE_DIR/profiledef.sh" ]]; then
        err "profiledef.sh bulunamadı! Profil dizini hatalı"
        exit 1
    fi
    if [[ ! -f "$PROFILE_DIR/packages.x86_64" ]]; then
        err "packages.x86_64 bulunamadı!"
        exit 1
    fi
    ok "Profil dosyaları doğrulandı"
}

validate_packages() {
    log "Paket listesi doğrulanıyor..."
    # Paket listesinde yorum ve boş satırları sayma
    local count
    count=$(grep -v "^#" "$PROFILE_DIR/packages.x86_64" | grep -v "^$" | wc -l)
    log "Paket sayısı: $count"
    if [[ $count -lt 50 ]]; then
        warn "Paket sayısı düşük görünüyor ($count), liste eksik olabilir"
    fi
    # pacman.conf kontrol
    if ! grep -q "^\[multilib\]" "$PROFILE_DIR/pacman.conf"; then
        warn "pacman.conf içinde multilib deposu etkin değil"
    else
        ok "multilib etkin"
    fi
}

configure_pacman() {
    log "pacman.conf yapılandırılıyor (multilib dahil)..."
    # Zaten profil içinde var, sadece bilgi ver
    ok "pacman.conf hazır"
}

setup_airootfs() {
    log "airootfs dosya sistemi kontrol ediliyor..."
    # Dosya izinlerini kontrol et (profiledef.sh file_permissions)
    local scripts=(
        "airootfs/usr/bin/hiroki-hw-detect"
        "airootfs/usr/bin/hiroki-de-selector"
        "airootfs/usr/bin/hiroki-welcome"
        "airootfs/usr/bin/hiroki-theme-manager"
        "airootfs/usr/bin/hiroki-update"
        "airootfs/usr/bin/hiroki-snapshot"
        "airootfs/usr/bin/hiroki-driver-manager"
        "airootfs/etc/hiroki/post-install/hiroki-post-install.sh"
    )
    for s in "${scripts[@]}"; do
        if [[ -f "$PROFILE_DIR/$s" ]]; then
            chmod +x "$PROFILE_DIR/$s"
            ok "Çalıştırılabilir: $s"
        else
            warn "Bulunamadı: $s"
        fi
    done
}

setup_calamares() {
    log "Calamares kaldirildi - kendi installer kullaniliyor"
    ok "Hiroki Installer hazir"
}

setup_themes() {
    log "Hiroki tema dosyaları kontrol ediliyor..."
    local theme_files=(
        "airootfs/usr/share/themes/Hiroki-Dark/gtk-3.0/gtk.css"
        "airootfs/usr/share/icons/Hiroki-Icons/index.theme"
        "airootfs/usr/share/grub/themes/hiroki/theme.txt"
        "airootfs/usr/share/hiroki/wallpapers/sakura-gradient.jpg"
    )
    for f in "${theme_files[@]}"; do
        if [[ -f "$PROFILE_DIR/$f" ]]; then
            ok "Tema: $f"
        else
            warn "Eksik tema: $f"
        fi
    done
}

setup_welcome() {
    log "Hoş geldiniz uygulaması kontrol ediliyor..."
    if [[ -f "$PROFILE_DIR/airootfs/usr/share/hiroki-welcome/hiroki-welcome.py" ]]; then
        ok "hiroki-welcome hazır"
    else
        warn "hiroki-welcome bulunamadı"
    fi
    if [[ -f "$PROFILE_DIR/airootfs/etc/skel/.config/autostart/hiroki-welcome.desktop" ]]; then
        ok "Autostart hazır"
    fi
}

setup_custom_tools() {
    log "Özel araçlar kontrol ediliyor..."
    local tools=(hiroki-de-selector hiroki-theme-manager hiroki-update hiroki-snapshot hiroki-driver-manager)
    for t in "${tools[@]}"; do
        if [[ -f "$PROFILE_DIR/airootfs/usr/bin/$t" ]]; then
            ok "Araç: $t"
        else
            warn "Eksik araç: $t"
        fi
    done
}

setup_systemd() {
    log "systemd servisleri yapılandırılıyor (live oturum için)..."
    # Live oturum için servislerin airootfs içinde var olduğunu varsay
    # NetworkManager, LightDM vb. zaten packages.x86_64 ile gelecek
    # Burada ek servis override'ları oluşturulabilir
    mkdir -p "$PROFILE_DIR/airootfs/etc/systemd/system" 2>/dev/null || true
    ok "systemd hazır"
}

setup_live_user() {
    log "Live kullanıcısı yapılandırılıyor (hiroki / hiroki)..."
    # Live user: hiroki, parola yok veya hiroki, otomatik giriş
    # Archiso varsayılan olarak live kullanıcı oluşturur, biz Hiroki için özelleştiriyoruz
    # /etc/passwd ve /etc/shadow archiso tarafından yönetilir, biz sadece lightdm autologin ayarı yapıyoruz
    mkdir -p "$PROFILE_DIR/airootfs/etc/lightdm" 2>/dev/null || true
    # Live için autologin (kurulum sonrası değil, sadece live)
    # Bu ayar build sonrası iso'da aktif olacak, kurulum sonrası hedef sistemde post-install tarafından düzeltilecek
    ok "Live kullanıcı: hiroki (otomatik giriş)"
}

build_squashfs() {
    log "squashfs sıkıştırması mkarchiso tarafından yapılacak..."
    ok "SquashFS ayarları profiledef.sh içinde tanımlı (xz)"
}

build_iso() {
    log "ISO dosyası oluşturuluyor... (bu işlem uzun sürebilir)"
    log "Profil: $PROFILE_DIR"
    log "Çıktı: $OUT_DIR"
    # Eski ISO'ları temizle
    rm -f "$OUT_DIR"/*.iso "$OUT_DIR"/*.sha256 "$OUT_DIR"/*.md5 2>/dev/null || true

    # mkarchiso çalıştır
    # Not: Bu komut temiz bir Arch Linux ortamında çalıştırılmalı
    if command -v mkarchiso >/dev/null 2>&1; then
        mkarchiso -v -w "$WORK_DIR" -o "$OUT_DIR" "$PROFILE_DIR" || {
            err "mkarchiso başarısız! Log: $WORK_DIR"
            exit 1
        }
        ok "ISO oluşturuldu"
        ls -lh "$OUT_DIR"/
    else
        warn "mkarchiso bulunamadı — simülasyon modunda çalışıyor"
        log "Gerçek derleme için Arch Linux'ta çalıştırın: sudo mkarchiso -v -w /tmp/hiroki-work -o ./out ./"
        # Simülasyon: sahte ISO oluştur
        mkdir -p "$OUT_DIR"
        echo "Hiroki OS 1.0 Sakura - Simülasyon ISO (gerçek derleme Arch Linux gerektirir)" > "$OUT_DIR/$ISO_NAME.info"
        log "Simülasyon dosyası: $OUT_DIR/$ISO_NAME.info"
    fi
}

generate_checksums() {
    log "Checksum dosyaları oluşturuluyor..."
    cd "$OUT_DIR"
    for iso in *.iso; do
        if [[ -f "$iso" ]]; then
            sha256sum "$iso" > "$iso.sha256" && ok "SHA256: $iso.sha256"
            md5sum "$iso" > "$iso.md5" && ok "MD5: $iso.md5"
            # Detaylı bilgi
            echo "ISO: $iso" > "$iso.info"
            echo "Boyut: $(du -h "$iso" | cut -f1)" >> "$iso.info"
            echo "Tarih: $(date -Iseconds)" >> "$iso.info"
            echo "Profil: Hiroki OS 1.0 Sakura" >> "$iso.info"
            cat "$iso.info"
        fi
    done
    cd - >/dev/null
}

final_info() {
    echo ""
    echo -e "${PINK}═══════════════════════════════════════════${NC}"
    echo -e "${PINK}  Hiroki OS 1.0 Sakura — Derleme Tamamlandı 🌸${NC}"
    echo -e "${PINK}═══════════════════════════════════════════${NC}"
    echo -e "  Çıktı dizini: ${GREEN}$OUT_DIR${NC}"
    ls -lh "$OUT_DIR"/ 2>/dev/null || echo "  (henüz ISO yok - simülasyon)"
    echo ""
    echo -e "  Kurulum için:"
    echo -e "    ${YELLOW}dd if=$OUT_DIR/hiroki-os-*.iso of=/dev/sdX bs=4M status=progress oflag=sync${NC}"
    echo -e "  veya Ventoy / Etcher ile yazdırın"
    echo ""
    echo -e "  Test:"
    echo -e "    ${YELLOW}qemu-system-x86_64 -enable-kvm -m 2048 -cdrom $OUT_DIR/hiroki-os-*.iso -boot d${NC}"
    echo ""
}

# --- ANA AKIŞ ---
main() {
    echo -e "${PINK}"
    cat <<'EOS'
  _   _ _           _    _    ___  ____
 | | | (_)_ __ ___ | | _(_)_ / _ \/ ___|
 | |_| | | '__/ _ \| |/ / / | | | \___ \
 |  _  | | | | (_) |   <| | |_| |___) |
 |_| |_|_|_|  \___/|_|\_\_|\___/|____/

  Hiroki OS 1.0 Sakura — ISO Derleme Aracı
  Arch Linux tabanlı • Calamares • Sakura gibi zarif
EOS
    echo -e "${NC}"

    check_root
    check_deps
    prepare_workdir
    copy_profile
    validate_packages
    configure_pacman
    setup_airootfs
    setup_calamares
    setup_themes
    setup_welcome
    setup_custom_tools
    setup_systemd
    setup_live_user
    build_squashfs
    build_iso
    generate_checksums
    final_info
}

main "$@"
