# Linis — Geliştirici Notları

## Mimari Özet

```
linis (tek process: WM + compositor)
├── linis.c               giriş, X olay döngüsü (select)
├── linis_wm.c/.h         pencere yönetimi, workspace, düzen, keybind
├── linis_compositor.c/.h XRender compositor (gölge/köşe/blur/anim)
├── linis_xsurf.c/.h      ARGB32 yüzeyler + maske/blur yardımcıları
├── linis_ewmh.c/.h       _NET_* / ICCCM atom & property işlemleri
├── linis_keybind.c/.h    keybinds.conf parser + mod-watch (launcher)
├── linis_config.c/.h     TOML alt-kümesi parser (tema/config)
├── linis_anim.c/.h       easing + tween
├── linis_util.c/.h       bellek/log/spawn/string yardımcıları
linis-panel               sol panel + alt bar (EWMH okur)
linis-launch              .desktop tarayıcı + filtreleme
linis-session / -restart  shell scriptler
```

## Anahtar Kararlar

- **Xlib**, XCB değil; tek bağımlılık ailesi: libX11 + XRender + XComposite
  + XDamage (XRender compositor için gereklidir).
- Compositor alt pencere yönlendirmesi (`XCompositeRedirectSubwindows`)
  kullanır; her kare: wallpaper katmanı → alttan üste pencereler (gölge,
  köşe maskesi, alfa, blur) → root'a `PictOpSrc`.
- Tüm iç yüzeyler **ARGB32**. CPU piksel yazımlarında her byte aynı değere
  eşitlenir → sunucunun 32-bit görsel kanal sıralamasından bağımsız alfa.
- Maskeler yuvarlak köşe kapsamasını kenar yumuşatmalı hesaplar; aynı boyut
  + yarıçap + alfa için önbelleklenir.
- "Super tek basımı → launcher": Super tuşu pasif grab ile yakalanır;
  mod-watch, tuş **bırakıldığında** başka tuş basılmadıysa launcher açar.
- **Boot video** (`linis_bootvid.c`, Super+V): `/usr/share/linis/videos`
  altından ilk videoyu mpv/vlc/ffplay ile tam ekran-sessiz oynatır.
  mpv'nin tam ekran isteği `_NET_WM_STATE` ClientMessage olarak gelir;
  WM (`on_clientmessage`) bunu `client_fullscreen()` ile onaylar ve
  `_NET_WM_STATE` özelliğini gerçekten yazar (EWMH uyumlu uygulamalar için
  tam ekran desteği böylece genelleşir).
- Workarea, görünür dock pencerelerinin `_NET_WM_STRUT` değerlerinden türetilir.
- Animasyonlar tween tabanlıdır; compositor animasyon karesinde tam yeniden
  çizim yapar (basitlik; ileride bölgesel iyileştirilebilir).

## Yapılacaklar / İyileştirme Fikirleri

1. Bölgesel (damage-area) yeniden çizim → CPU kazanımı.
2. Gölgeyi XRender küçült-büyüt ile hesapla (CPU blur'u kaldır).
3. `_NET_WM_STATE` client isteklerine tam cevap (maximize vb.).
4. Tepside gerçek XEmbed sistemi tepsisi (XEMBED) — şu an yok.
5. Fareyle workspace'e sürükle (panoda) → `_NET_WM_DESKTOP` ClientMessage
   WM'de hazır.
6. Aktif pencere glow (compositor'da yorum satırı olarak bırakıldı).
