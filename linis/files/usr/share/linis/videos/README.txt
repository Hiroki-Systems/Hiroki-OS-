Linis açılış (boot) videoları bu klasöre konur.

- WM her açılışta (ya da Super+V ile) bu klasördeki ilk videoyu
  tam ekran + sessiz oynatır.
- Süre üst sınırı ~/.config/linis/config.toml içindeki [video]
  bölümünden ayarlanır (duration_sec, varsayılan 12).
- Desteklenen uzantılar: .mp4 .webm .mkv .avi .mov .m4v .ogv
- Oynatıcı: mpv (yoksa vlc → ffplay). mpv'nin tam ekran isteği WM'in
  _NET_WM_STATE desteğiyle çalışır.

Örnek:
    sudo cp benim-intro.mp4 /usr/share/linis/videos/
    # sonra oturumu yeniden başlat (Super+Shift+R)
