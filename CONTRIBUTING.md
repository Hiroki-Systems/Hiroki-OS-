# 🤝 Katkıda Bulunma — Hiroki OS

Sakura yaprakları gibi her katkı değerlidir. Teşekkürler!

---

## 1. Nasıl Katkıda Bulunulur

### Hata Bildir (Issue)

- https://github.com/hiroki-os/hiroki-os/issues → **New Issue**
- Şablon: Başlık, açıklama, adımlar, beklenen/gerçek sonuç, `hiroki-hw-detect` çıktısı, Calamares log (`/var/log/calamares/`)

### Özellik İsteği

- Aynı issue sayfasında **Feature Request** etiketiyle açın; öneri matrisini veya yeni DE'yi tartışalım

### Kod Katkısı (Pull Request)

1. Fork → `git clone https://github.com/<sen>/hiroki-os.git`
2. Branch: `git checkout -b feature/kisa-aciklama`
3. Değiştir, test et (QEMU/VirtualBox)
4. Commit: Conventional Commits (`feat:`, `fix:`, `docs:`, `style:`)
   ```
   feat(de-selector): Hyprland için VRAM kontrolü eklendi
   fix(calamares): btrfs subvolume compress bayrağı düzeltildi
   ```
5. Push → GitHub'da **Pull Request** aç, şablonu doldur

---

## 2. Kod Stili

- **Bash:** `shellcheck` temiz, `set -euo pipefail`, shebang `#!/usr/bin/env bash`, fonksiyonlar `snake_case`
- **Python (GTK):** PEP8, `python -m py_compile`, Türkçe yorumlarda UTF-8, `gi.require_version` doğru
- **Calamares YAML:** 2 boşluk girinti, resmi Calamares anahtarları (bkz. https://github.com/calamares/calamares)
- **Archiso:** Resmi Arch Wiki ile uyumlu; paket isimleri `pacman -Si` ile doğrulanmış
- **Tema CSS:** Renkler sadece Hiroki paleti (`#2D1B69`, `#E91E8C`, `#00D4AA`, `#0D0D1A`, `#1A1A2E`)
- **Commit mesajı:** Türkçe veya İngilizce, açıklayıcı, tek satır özet + detaylı gövde

---

## 3. Test

PR öncesi kontrol listesi (bkz. README → Kalite Güvencesi, 21 madde):

- [ ] `shellcheck build.sh airootfs/usr/bin/hiroki-*`
- [ ] `python3 -m py_compile airootfs/usr/share/hiroki-welcome/*.py`
- [ ] `yamllint airootfs/etc/calamares/**/*.conf` (opsiyonel)
- [ ] QEMU'da 512 MB, 2 GB, 8 GB RAM ile ayrı test
- [ ] Calamares tüm adımları geçiyor, kurulum sonrası boot ediyor

---

## 4. İletişim

- Tartışma: GitHub Discussions
- E-posta: hiroki-os@example.org (örnek, gerçek adres repo'da)
- Davranış Kuralları: Saygılı, kapsayıcı, ayrımcılık yok (Contributor Covenant)

---

## 5. Lisans

Katkılarınız GPL-3.0 altında lisanslanır. Bkz. LICENSE.

---

Arigato! 🌸
