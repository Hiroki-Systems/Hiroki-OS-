#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Hiroki OS - Hoş Geldiniz Uygulaması
Sakura 1.0 - hiroki-welcome
Python + GTK3
Live oturum ve kurulum sonrası ilk açılış için iki mod destekler.
"""

import json
import os
import subprocess
import sys

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, Gdk, GLib

HW_JSON = "/tmp/hiroki-hw-info.json"
INSTALLED_FLAG = "/var/lib/hiroki/first-boot-done"

def is_live_session():
    # Live oturum kontrolü: /run/archiso veya kullanıcı hiroki ve live flag
    return os.path.exists("/run/archiso/bootmnt") or os.path.exists("/run/miso/bootmnt") or os.path.exists("/tmp/hiroki-live-mode")

def load_hw():
    try:
        if os.path.exists(HW_JSON):
            with open(HW_JSON, 'r') as f:
                return json.load(f)
    except:
        pass
    return None

def run_cmd(cmd):
    try:
        subprocess.Popen(cmd, shell=True)
    except Exception as e:
        print(e, file=sys.stderr)

class HirokiWelcome(Gtk.Window):
    def __init__(self, live=True):
        super().__init__(title="Hiroki OS'a Hoş Geldiniz")
        self.live = live
        self.set_default_size(880, 620)
        self.set_position(Gtk.WindowPosition.CENTER)
        self.set_border_width(0)
        self.connect("destroy", Gtk.main_quit)
        self.apply_css()
        self.build_ui()

    def apply_css(self):
        css = b"""
        window { background-color: #0D0D1A; }
        .hero { background: linear-gradient(135deg, #2D1B69 0%, #1A1A2E 50%, #E91E8C 100%); border-radius: 16px; padding: 20px; }
        .card { background-color: #1A1A2E; border-radius: 12px; border: 1px solid #2D1B69; padding: 14px; }
        .btn-primary { background: #E91E8C; color: white; border-radius: 10px; padding: 12px 20px; font-weight: bold; font-size: 13px; }
        .btn-secondary { background: #2D1B69; color: #EAEAEA; border-radius: 10px; padding: 10px 18px; }
        .btn-ghost { background: transparent; color: #A0A0B8; border: 1px solid #2D1B69; border-radius: 10px; padding: 8px 16px; }
        .title { color: #EAEAEA; font-size: 22px; font-weight: 800; }
        .subtitle { color: #A0A0B8; font-size: 12px; }
        .hw { color: #EAEAEA; font-size: 11px; }
        """
        provider = Gtk.CssProvider()
        provider.load_from_data(css)
        Gtk.StyleContext.add_provider_for_screen(Gdk.Screen.get_default(), provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)

    def build_ui(self):
        main = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        self.add(main)

        # Hero
        hero = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=16)
        hero.get_style_context().add_class("hero")
        hero.set_margin_top(16)
        hero.set_margin_start(16)
        hero.set_margin_end(16)
        hero.set_margin_bottom(12)
        logo = Gtk.Label()
        logo.set_markup('<span size="40000">🌸</span>')
        hero.pack_start(logo, False, False, 0)
        text_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4)
        if self.live:
            title = "Hiroki OS'a Hoş Geldiniz"
            sub = "Sakura 1.0  •  Arch tabanlı, sakura gibi zarif"
        else:
            title = "Hiroki OS'unuz Hazır! 🎉"
            sub = "Sakura 1.0 başarıyla kuruldu  •  Keyfini çıkarın"
        t1 = Gtk.Label()
        t1.set_markup(f'<span class="title" color="white">{title}</span>')
        t1.set_halign(Gtk.Align.START)
        t2 = Gtk.Label(label=sub)
        t2.get_style_context().add_class("subtitle")
        t2.set_halign(Gtk.Align.START)
        # Override subtitle color for hero
        t2.set_markup(f'<span color="#EAEAEA" size="10000">{sub}</span>')
        text_box.pack_start(t1, False, False, 0)
        text_box.pack_start(t2, False, False, 0)
        hero.pack_start(text_box, True, True, 0)
        # Sağda sürüm badge
        ver = Gtk.Label()
        ver.set_markup('<span background="#00D4AA" color="#0D0D1A" weight="bold" size="9000">  v1.0 Sakura  </span>')
        hero.pack_end(ver, False, False, 0)
        main.pack_start(hero, False, False, 0)

        content = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=16)
        content.set_margin_start(16)
        content.set_margin_end(16)
        content.set_margin_bottom(16)
        content.set_vexpand(True)
        main.pack_start(content, True, True, 0)

        # Sol panel: donanım + hızlı eylemler
        left = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12)
        left.set_size_request(300, -1)
        content.pack_start(left, False, False, 0)

        # Donanım kartı
        hw_card = Gtk.Frame()
        hw_card.get_style_context().add_class("card")
        hw_inner = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        hw_inner.set_margin_top(12)
        hw_inner.set_margin_bottom(12)
        hw_inner.set_margin_start(12)
        hw_inner.set_margin_end(12)
        hw_title = Gtk.Label()
        hw_title.set_markup('<b><span color="#00D4AA">💻 Donanım Özeti</span></b>')
        hw_title.set_halign(Gtk.Align.START)
        hw_inner.pack_start(hw_title, False, False, 0)
        hw_inner.pack_start(Gtk.Separator(), False, False, 2)
        hw = load_hw()
        if hw:
            def row(lbl, val):
                r = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=6)
                l = Gtk.Label()
                l.set_markup(f'<span color="#A0A0B8" size="9000">{lbl}</span>')
                l.set_halign(Gtk.Align.START)
                v = Gtk.Label()
                v.set_markup(f'<span color="#EAEAEA" size="9000" weight="bold">{GLib.markup_escape_text(str(val))}</span>')
                v.set_halign(Gtk.Align.END)
                v.set_line_wrap(True)
                r.pack_start(l, True, True, 0)
                r.pack_start(v, False, False, 0)
                return r
            hw_inner.pack_start(row("RAM", f"{hw.get('ram_mb')} MB"), False, False, 0)
            hw_inner.pack_start(row("CPU", f"{hw.get('cpu_cores')} çekirdek"), False, False, 0)
            hw_inner.pack_start(row("GPU", hw.get('gpu_vendor','-')), False, False, 0)
            hw_inner.pack_start(row("Disk", f"{hw.get('disk_gb')} GB"), False, False, 0)
            rec = hw.get('recommended','xfce')
            hw_inner.pack_start(row("Öneri", rec.upper()), False, False, 0)
            msg = Gtk.Label()
            msg.set_markup(f'<span color="#A0A0B8" size="8000"><i>{GLib.markup_escape_text(hw.get("message",""))}</i></span>')
            msg.set_line_wrap(True)
            msg.set_halign(Gtk.Align.START)
            hw_inner.pack_start(msg, False, False, 4)
        else:
            lbl = Gtk.Label(label="Donanım bilgisi yükleniyor...")
            lbl.get_style_context().add_class("subtitle")
            hw_inner.pack_start(lbl, False, False, 0)
            # hw-detect çalıştır
            run_cmd("/usr/bin/hiroki-hw-detect >/tmp/hiroki-hw-info.json 2>&1 &")
        hw_card.add(hw_inner)
        left.pack_start(hw_card, False, False, 0)

        # Hızlı bağlantılar kartı
        link_card = Gtk.Frame()
        link_card.get_style_context().add_class("card")
        link_inner = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        link_inner.set_margin_top(12)
        link_inner.set_margin_bottom(12)
        link_inner.set_margin_start(12)
        link_inner.set_margin_end(12)
        link_title = Gtk.Label()
        link_title.set_markup('<b><span color="#E91E8C">🔗 Hızlı Bağlantılar</span></b>')
        link_title.set_halign(Gtk.Align.START)
        link_inner.pack_start(link_title, False, False, 0)
        links = [
            ("📖  Belgeler", "https://wiki.hiroki-os.org"),
            ("💬  Topluluk / Forum", "https://hiroki-os.org/community"),
            ("🐛  Hata Bildir", "https://github.com/hiroki-os/hiroki-os/issues"),
            ("💖  Bağış Yap", "https://hiroki-os.org/donate"),
        ]
        for label, url in links:
            btn = Gtk.Button(label=label)
            btn.get_style_context().add_class("btn-ghost")
            btn.set_halign(Gtk.Align.FILL)
            btn.connect("clicked", lambda w, u=url: run_cmd(f"xdg-open {u} &"))
            link_inner.pack_start(btn, False, False, 0)
        link_card.add(link_inner)
        left.pack_start(link_card, False, False, 0)

        # Sağ panel: eylem kartları
        right = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12)
        right.set_vexpand(True)
        content.pack_start(right, True, True, 0)

        if self.live:
            # Live mod kartları
            cards = [
                {
                    "icon": "🎨",
                    "title": "Masaüstü Ortamı Seç",
                    "desc": "Donanımınıza göre önerilen ortamı görün, 10 seçenekten dilediğinizi seçin. Öneririz, asla zorlamayız.",
                    "btn": "DE Seçiciyi Aç",
                    "action": lambda w: run_cmd("/usr/bin/hiroki-de-selector &"),
                    "primary": True,
                },
                {
                    "icon": "💿",
                    "title": "Kuruluma Başla",
                    "desc": "Calamares yükleyici ile diskinize kurun. Btrfs, EFI, Türkçe tam destek.",
                    "btn": "Calamares'i Başlat",
                    "action": lambda w: run_cmd("sudo -E calamares &"),
                    "primary": True,
                },
                {
                    "icon": "ℹ️",
                    "title": "Hiroki OS Hakkında",
                    "desc": "Sakura 1.0 yenilikleri, renk paleti, tema sistemi ve felsefemizi keşfedin.",
                    "btn": "Hakkında",
                    "action": self.on_about,
                    "primary": False,
                },
            ]
        else:
            cards = [
                {
                    "icon": "🔄",
                    "title": "Sistemi Güncelle",
                    "desc": "En son Arch paketleri ve Hiroki güncellemelerini alın. AUR hazır.",
                    "btn": "Güncelle",
                    "action": lambda w: run_cmd("/usr/bin/hiroki-update &"),
                    "primary": True,
                },
                {
                    "icon": "🎨",
                    "title": "Görünümü Özelleştir",
                    "desc": "Hiroki-Dark, duvar kağıtları, vurgu rengi ve ikonlar arasında geçiş yapın.",
                    "btn": "Tema Yöneticisi",
                    "action": lambda w: run_cmd("/usr/bin/hiroki-theme-manager &"),
                    "primary": False,
                },
                {
                    "icon": "🧩",
                    "title": "Ek Yazılım Kur",
                    "desc": "Pamac ile binlerce paket ve AUR desteği. Kurulu değilse nasıl kuracağınızı gösterir.",
                    "btn": "Paket Yöneticisi",
                    "action": self.on_pkg_manager,
                    "primary": False,
                },
                {
                    "icon": "💾",
                    "title": "Sistem Yedeği",
                    "desc": "Btrfs anlık görüntüleri ve Timeshift ile sisteminizi güvenceye alın.",
                    "btn": "Yedek Oluştur",
                    "action": lambda w: run_cmd("/usr/bin/hiroki-snapshot &"),
                    "primary": False,
                },
            ]

        for c in cards:
            card = Gtk.Frame()
            card.get_style_context().add_class("card")
            inner = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
            inner.set_margin_top(14)
            inner.set_margin_bottom(14)
            inner.set_margin_start(14)
            inner.set_margin_end(14)
            icon = Gtk.Label()
            icon.set_markup(f'<span size="24000">{c["icon"]}</span>')
            inner.pack_start(icon, False, False, 0)
            txt = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4)
            t = Gtk.Label()
            t.set_markup(f'<b><span color="#EAEAEA" size="12000">{c["title"]}</span></b>')
            t.set_halign(Gtk.Align.START)
            d = Gtk.Label()
            d.set_markup(f'<span color="#A0A0B8" size="9000">{c["desc"]}</span>')
            d.set_line_wrap(True)
            d.set_max_width_chars(42)
            d.set_halign(Gtk.Align.START)
            txt.pack_start(t, False, False, 0)
            txt.pack_start(d, False, False, 0)
            inner.pack_start(txt, True, True, 0)
            btn = Gtk.Button(label=c["btn"])
            btn.get_style_context().add_class("btn-primary" if c["primary"] else "btn-secondary")
            btn.set_valign(Gtk.Align.CENTER)
            btn.connect("clicked", c["action"])
            inner.pack_end(btn, False, False, 0)
            card.add(inner)
            right.pack_start(card, False, False, 0)

        # Alt çubuk
        footer = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        footer.set_margin_top(8)
        self.dont_show = Gtk.CheckButton(label="Bir daha gösterme")
        self.dont_show.get_style_context().add_class("subtitle")
        footer.pack_start(self.dont_show, False, False, 0)
        footer.pack_start(Gtk.Box(), True, True, 0)
        close_btn = Gtk.Button(label="Kapat")
        close_btn.get_style_context().add_class("btn-ghost")
        close_btn.connect("clicked", self.on_close)
        footer.pack_end(close_btn, False, False, 0)
        if self.live:
            remind_btn = Gtk.Button(label="Daha sonra hatırlat")
            remind_btn.get_style_context().add_class("btn-secondary")
            remind_btn.connect("clicked", lambda w: self.destroy())
            footer.pack_end(remind_btn, False, False, 0)
        right.pack_end(footer, False, False, 6)

    def on_pkg_manager(self, btn):
        import shutil
        for name in ("pamac-manager", "octopi"):
            if shutil.which(name):
                run_cmd(f"{name} &")
                return
        dialog = Gtk.MessageDialog(
            transient_for=self,
            flags=0,
            message_type=Gtk.MessageType.INFO,
            buttons=Gtk.ButtonsType.OK,
            text="Pamac yüklü değil 🌸"
        )
        dialog.format_secondary_text(
            "Pamac (pamac-aur) artık resmi Arch depolarında bulunmuyor, yalnız AUR'da.\n\n"
            "Kurmak için terminalde:\n"
            "  git clone https://aur.archlinux.org/pamac-aur.git\n"
            "  cd pamac-aur && makepkg -si\n\n"
            "Yay kullanıyorsanız: yay -S pamac-aur"
        )
        dialog.run()
        dialog.destroy()

    def on_about(self, btn):
        dialog = Gtk.MessageDialog(
            transient_for=self,
            flags=0,
            message_type=Gtk.MessageType.INFO,
            buttons=Gtk.ButtonsType.OK,
            text="Hiroki OS 1.0 Sakura 🌸"
        )
        dialog.format_secondary_text(
            "Arch Linux tabanlı, Calamares yükleyicili modern dağıtım.\n\n"
            "Felsefe: Donanımını tanı, en uygun masaüstünü öner ama asla zorla.\n"
            "• 10 masaüstü / WM desteği (XFCE, KDE, GNOME, Cinnamon, MATE, Budgie, LXQt, i3wm, Openbox, Hyprland)\n"
            "• Hiroki-Dark GTK, Hiroki-Icons, 5 duvar kağıdı\n"
            "• Türkçe varsayılan, çok dilli\n"
            "• Renkler: #2D1B69, #E91E8C, #00D4AA\n\n"
            "https://hiroki-os.org\n"
            "GPLv3 • Hiroki OS Project"
        )
        dialog.run()
        dialog.destroy()

    def on_close(self, btn):
        if self.dont_show.get_active():
            # Autostart'ı devre dışı bırak
            try:
                os.makedirs(os.path.expanduser("~/.config/autostart"), exist_ok=True)
                # Masaüstü dosyasını gizle
                autostart_path = os.path.expanduser("~/.config/autostart/hiroki-welcome.desktop")
                with open(autostart_path, "w") as f:
                    f.write("[Desktop Entry]\nHidden=true\n")
                # Sistem geneli flag
                if not self.live:
                    run_cmd("touch /var/lib/hiroki/first-boot-done 2>/dev/null || true")
            except Exception as e:
                print(e)
        self.destroy()
        Gtk.main_quit()

def main():
    live = is_live_session()
    # Kurulum sonrası ama flag varsa gösterme (kullanıcı daha önce kapatmış)
    if not live and os.path.exists(INSTALLED_FLAG):
        # yine de --force ile gösterilebilir
        if "--force" not in sys.argv:
            print("İlk açılış tamamlandı, hoş geldiniz gösterilmiyor. --force ile açabilirsiniz.")
            return
    win = HirokiWelcome(live=live)
    win.show_all()
    Gtk.main()

if __name__ == "__main__":
    main()
