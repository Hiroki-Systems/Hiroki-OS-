#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Hiroki OS - Akıllı Masaüstü Ortamı Seçici (GUI)
Sakura 1.0
Python + GTK3 (PyGObject)
Hem Live oturumda hem kurulum sonrası çalışır.
Felsefe: Öneririz, asla zorlamayız.
"""

import json
import os
import subprocess
import sys
import gi
gi.require_version('Gtk', '3.0')
gi.require_version('Gdk', '3.0')
from gi.repository import Gtk, Gdk, GLib, Pango

HW_JSON = "/tmp/hiroki-hw-info.json"
CONFIG_JSON = "/etc/hiroki/de-selector/recommendations.json"

DESKTOPS = {
    "xfce":      {"name": "XFCE",       "desc": "Hafif, hızlı ve dengeli. Orta seviye donanımlar için mükemmel.", "icon": "xfce4-panel", "color": "#00D4AA"},
    "kde":       {"name": "KDE Plasma", "desc": "Modern, özelleştirilebilir ve tam özellikli. Güçlü donanımlar için.", "icon": "plasma", "color": "#2D1B69"},
    "gnome":     {"name": "GNOME",      "desc": "Sade, zarif ve dokunmatik dostu. Modern iş akışı.", "icon": "gnome", "color": "#E91E8C"},
    "cinnamon":  {"name": "Cinnamon",   "desc": "Geleneksel masaüstü sevenler için. GNOME tabanlı, klasik.", "icon": "cinnamon", "color": "#2D1B69"},
    "mate":      {"name": "MATE",       "desc": "GNOME 2'nin devamı. Son derece hafif ve stabil.", "icon": "mate", "color": "#00D4AA"},
    "budgie":    {"name": "Budgie",     "desc": "Modern ve şık. GNOME teknolojileri üzerine zarif.", "icon": "budgie", "color": "#E91E8C"},
    "lxqt":      {"name": "LXQt",       "desc": "Ultra hafif Qt tabanlı. 512MB-2GB için birinci öneri.", "icon": "lxqt", "color": "#00D4AA"},
    "i3wm":      {"name": "i3wm",       "desc": "Klavye odaklı tiling WM. İleri düzey, çok düşük RAM.", "icon": "i3", "color": "#A0A0B8"},
    "openbox":   {"name": "Openbox",    "desc": "Minimal pencere yöneticisi. 512MB altı için önerilir.", "icon": "openbox", "color": "#A0A0B8"},
    "hyprland":  {"name": "Hyprland",   "desc": "Modern Wayland tiling compositor. Animasyonlar ve blur.", "icon": "hyprland", "color": "#E91E8C"},
}

RECOMMENDED_STYLES = {
    "minimal": "Donanımınız çok sınırlı, pencere yöneticisi kullanmanız önerilir",
    "light": "Donanımınız hafif bir masaüstü ortamı için uygun",
    "medium": "Donanımınız orta seviye masaüstü ortamları için uygun",
    "full": "Donanımınız tam özellikli masaüstü ortamları için uygun",
    "high": "Donanımınız tüm masaüstü ortamlarını rahatça çalıştırabilir",
}

def run_hw_detect():
    try:
        subprocess.run(["/usr/bin/hiroki-hw-detect"], check=True, stdout=subprocess.DEVNULL)
    except Exception as e:
        print(f"hw-detect hatası: {e}", file=sys.stderr)

def load_hw():
    if not os.path.exists(HW_JSON):
        run_hw_detect()
    try:
        with open(HW_JSON, 'r') as f:
            return json.load(f)
    except:
        return {
            "ram_mb": 4096, "ram_gb": 4.0, "cpu_cores": 4, "cpu_model": "Bilinmeyen CPU",
            "gpu_raw": "Bilinmiyor", "gpu_vendor": "unknown", "disk_gb": 50,
            "virt": "none", "is_vm": False, "tier": "full",
            "primary": "kde", "secondary": "xfce,gnome", "message": RECOMMENDED_STYLES["full"],
            "recommended": "kde"
        }

class HirokiDESelector(Gtk.Window):
    def __init__(self):
        super().__init__(title="Hiroki OS - Masaüstü Ortamı Seçici")
        self.set_default_size(1000, 650)
        self.set_position(Gtk.WindowPosition.CENTER)
        self.set_border_width(0)
        self.hw = load_hw()
        self.selected_de = self.hw.get("recommended", "xfce")
        self.connect("destroy", Gtk.main_quit)
        self.apply_css()
        self.build_ui()

    def apply_css(self):
        css = b"""
        window { background-color: #0D0D1A; }
        .title { color: #EAEAEA; font-size: 22px; font-weight: bold; }
        .subtitle { color: #A0A0B8; font-size: 13px; }
        .card { background-color: #1A1A2E; border-radius: 12px; padding: 16px; border: 1px solid #2D1B69; }
        .card-recommended { background: linear-gradient(135deg, #2D1B69 0%, #E91E8C 100%); border-radius: 14px; padding: 18px; }
        .card-selected { border: 2px solid #00D4AA; }
        .btn-primary { background: #E91E8C; color: white; border-radius: 8px; padding: 10px 24px; font-weight: bold; }
        .btn-secondary { background: #2D1B69; color: #EAEAEA; border-radius: 8px; padding: 10px 24px; }
        .hw-label { color: #A0A0B8; font-size: 11px; }
        .hw-value { color: #EAEAEA; font-size: 12px; font-weight: bold; }
        .warning { background-color: #E91E8C; color: white; border-radius: 8px; padding: 12px; }
        """
        provider = Gtk.CssProvider()
        provider.load_from_data(css)
        Gtk.StyleContext.add_provider_for_screen(Gdk.Screen.get_default(), provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)

    def build_ui(self):
        main = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        self.add(main)

        # Header
        header = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        header.set_margin_top(16)
        header.set_margin_bottom(16)
        header.set_margin_start(20)
        header.set_margin_end(20)
        logo = Gtk.Label()
        logo.set_markup('<span size="28000" color="#E91E8C">🌸</span>')
        header.pack_start(logo, False, False, 0)
        titles = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
        t1 = Gtk.Label()
        t1.set_markup('<span class="title">Hiroki OS 1.0 Sakura</span>')
        t1.set_halign(Gtk.Align.START)
        t2 = Gtk.Label(label="Akıllı Masaüstü Seçici  •  Öneririz, asla zorlamayız")
        t2.get_style_context().add_class("subtitle")
        t2.set_halign(Gtk.Align.START)
        titles.pack_start(t1, False, False, 0)
        titles.pack_start(t2, False, False, 0)
        header.pack_start(titles, True, True, 0)
        # Donanım özet düğmesi
        main.pack_start(header, False, False, 0)
        main.pack_start(Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL), False, False, 0)

        content = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=16)
        content.set_margin_top(16)
        content.set_margin_bottom(16)
        content.set_margin_start(16)
        content.set_margin_end(16)
        main.pack_start(content, True, True, 0)

        # Sol: Donanım + Öneri
        left = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=16)
        left.set_size_request(320, -1)
        content.pack_start(left, False, False, 0)

        # Donanım kartı
        hw_frame = Gtk.Frame()
        hw_frame.get_style_context().add_class("card")
        hw_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        hw_box.set_margin_top(12)
        hw_box.set_margin_bottom(12)
        hw_box.set_margin_start(12)
        hw_box.set_margin_end(12)
        hw_title = Gtk.Label()
        hw_title.set_markup('<b><span color="#00D4AA">💻 Donanım Bilgisi</span></b>')
        hw_title.set_halign(Gtk.Align.START)
        hw_box.pack_start(hw_title, False, False, 0)
        hw_box.pack_start(Gtk.Separator(), False, False, 4)

        def hw_row(icon, label, value):
            row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
            l = Gtk.Label()
            l.set_markup(f'<span class="hw-label">{icon} {label}</span>')
            l.set_halign(Gtk.Align.START)
            v = Gtk.Label()
            v.set_markup(f'<span class="hw-value">{value}</span>')
            v.set_halign(Gtk.Align.END)
            v.set_ellipsize(Pango.EllipsizeMode.MIDDLE)
            row.pack_start(l, True, True, 0)
            row.pack_start(v, False, False, 0)
            return row

        hw_box.pack_start(hw_row("🧠", "RAM", f"{self.hw['ram_mb']} MB ({self.hw['ram_gb']:.1f} GB)"), False, False, 0)
        hw_box.pack_start(hw_row("⚙️", "CPU", f"{self.hw['cpu_cores']} çekirdek"), False, False, 0)
        cpu_model = self.hw.get('cpu_model', '')[:40]
        hw_box.pack_start(hw_row("🔧", "Model", cpu_model), False, False, 0)
        hw_box.pack_start(hw_row("🎮", "GPU", self.hw.get('gpu_vendor', 'unknown')), False, False, 0)
        hw_box.pack_start(hw_row("💾", "Disk", f"{self.hw['disk_gb']} GB"), False, False, 0)
        virt = self.hw.get('virt', 'none')
        virt_str = "Sanal Makine ("+virt+")" if virt != "none" else "Fiziksel Makine"
        hw_box.pack_start(hw_row("🖥️", "Ortam", virt_str), False, False, 0)
        gpu_raw = self.hw.get('gpu_raw','')[:50]
        if gpu_raw:
            lbl = Gtk.Label()
            lbl.set_markup(f'<span size="8000" color="#A0A0B8">{GLib.markup_escape_text(gpu_raw)}</span>')
            lbl.set_line_wrap(True)
            lbl.set_halign(Gtk.Align.START)
            hw_box.pack_start(lbl, False, False, 2)
        hw_frame.add(hw_box)
        left.pack_start(hw_frame, False, False, 0)

        # Öneri kartı
        rec_frame = Gtk.Frame()
        rec_frame.get_style_context().add_class("card-recommended")
        rec_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        rec_box.set_margin_top(16)
        rec_box.set_margin_bottom(16)
        rec_box.set_margin_start(16)
        rec_box.set_margin_end(16)
        rec_label = Gtk.Label()
        rec_label.set_markup('<span color="white" size="11000"><b>✨ ÖNERİLEN</b></span>')
        rec_label.set_halign(Gtk.Align.START)
        rec_box.pack_start(rec_label, False, False, 0)
        rec_de_id = self.hw.get("recommended", "xfce")
        rec_de = DESKTOPS.get(rec_de_id, DESKTOPS["xfce"])
        rec_name = Gtk.Label()
        rec_name.set_markup(f'<span color="white" size="20000" weight="bold">{rec_de["name"]}</span>')
        rec_name.set_halign(Gtk.Align.START)
        rec_box.pack_start(rec_name, False, False, 0)
        rec_desc = Gtk.Label()
        rec_desc.set_markup(f'<span color="#EAEAEA">{rec_de["desc"]}</span>')
        rec_desc.set_line_wrap(True)
        rec_desc.set_halign(Gtk.Align.START)
        rec_box.pack_start(rec_desc, False, False, 0)
        rec_msg = Gtk.Label()
        rec_msg.set_markup(f'<span color="white" size="9000"><i>{GLib.markup_escape_text(self.hw.get("message",""))}</i></span>')
        rec_msg.set_line_wrap(True)
        rec_msg.set_halign(Gtk.Align.START)
        rec_box.pack_start(rec_msg, False, False, 6)
        tier_lbl = Gtk.Label()
        tier_lbl.set_markup(f'<span color="#00D4AA" size="9000">Seviye: {self.hw.get("tier","-")}  •  {self.hw.get("ram_mb")} MB RAM</span>')
        tier_lbl.set_halign(Gtk.Align.START)
        rec_box.pack_start(tier_lbl, False, False, 0)
        rec_frame.add(rec_box)
        left.pack_start(rec_frame, False, False, 0)

        # Uyarı alanı (başlangıçta gizli)
        self.warning_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        self.warning_box.set_no_show_all(True)
        self.warning_label = Gtk.Label()
        self.warning_label.set_line_wrap(True)
        self.warning_box.pack_start(self.warning_label, False, False, 0)
        left.pack_start(self.warning_box, False, False, 0)

        # Sağ: Tüm masaüstleri
        right = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        content.pack_start(right, True, True, 0)
        right_title = Gtk.Label()
        right_title.set_markup('<b><span color="#EAEAEA" size="13000">Tüm Masaüstü Ortamları (10)</span></b>')
        right_title.set_halign(Gtk.Align.START)
        right.pack_start(right_title, False, False, 0)
        hint = Gtk.Label(label="Birine tıklayarak seçin — öneri dışında da seçim tamamen serbest!")
        hint.get_style_context().add_class("subtitle")
        hint.set_halign(Gtk.Align.START)
        right.pack_start(hint, False, False, 0)

        scroll = Gtk.ScrolledWindow()
        scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        scroll.set_vexpand(True)
        grid = Gtk.FlowBox()
        grid.set_valign(Gtk.Align.START)
        grid.set_max_children_per_line(2)
        grid.set_selection_mode(Gtk.SelectionMode.NONE)
        grid.set_row_spacing(10)
        grid.set_column_spacing(10)
        grid.set_homogeneous(True)
        scroll.add(grid)
        right.pack_start(scroll, True, True, 0)

        self.de_buttons = {}
        for de_id, info in DESKTOPS.items():
            card = Gtk.Button()
            card.set_relief(Gtk.ReliefStyle.NONE)
            is_recommended = (de_id == rec_de_id)
            # İçerik
            inner = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
            inner.set_margin_top(12)
            inner.set_margin_bottom(12)
            inner.set_margin_start(12)
            inner.set_margin_end(12)
            # Başlık
            title_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
            icon_lbl = Gtk.Label()
            # Basit emoji ikon haritası
            emoji = {"xfce":"🐭","kde":"🔷","gnome":"🦶","cinnamon":"🍂","mate":"🧉","budgie":"🐦","lxqt":"⚡","i3wm":"⬢","openbox":"📦","hyprland":"🌊"}.get(de_id,"💠")
            icon_lbl.set_markup(f'<span size="18000">{emoji}</span>')
            title_box.pack_start(icon_lbl, False, False, 0)
            name_lbl = Gtk.Label()
            name_lbl.set_markup(f'<b><span color="#EAEAEA">{info["name"]}</span></b>')
            name_lbl.set_halign(Gtk.Align.START)
            title_box.pack_start(name_lbl, True, True, 0)
            if is_recommended:
                badge = Gtk.Label()
                badge.set_markup('<span background="#00D4AA" color="#0D0D1A" weight="bold" size="8000">  ÖNERİLEN  </span>')
                title_box.pack_end(badge, False, False, 0)
            inner.pack_start(title_box, False, False, 0)
            desc_lbl = Gtk.Label()
            desc_lbl.set_markup(f'<span color="#A0A0B8" size="9000">{info["desc"]}</span>')
            desc_lbl.set_line_wrap(True)
            desc_lbl.set_max_width_chars(28)
            desc_lbl.set_halign(Gtk.Align.START)
            inner.pack_start(desc_lbl, False, False, 0)
            card.add(inner)
            # Stil
            ctx = card.get_style_context()
            ctx.add_class("card")
            if de_id == self.selected_de:
                ctx.add_class("card-selected")
            # Event
            card.connect("clicked", self.on_de_selected, de_id)
            self.de_buttons[de_id] = card
            grid.add(card)

        # Alt butonlar
        btn_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        btn_box.set_margin_top(12)
        btn_box.set_halign(Gtk.Align.END)
        # Calamares modu kontrolü
        is_calamares = os.path.exists("/tmp/calamares-hiroki-mode")
        cancel_btn = Gtk.Button(label="Kapat")
        cancel_btn.get_style_context().add_class("btn-secondary")
        cancel_btn.connect("clicked", lambda w: Gtk.main_quit())
        btn_box.pack_start(cancel_btn, False, False, 0)

        if is_calamares:
            apply_btn = Gtk.Button(label="Seç ve Kuruluma Devam Et")
        else:
            apply_btn = Gtk.Button(label="Live Ortamda Başlat")
        apply_btn.get_style_context().add_class("btn-primary")
        apply_btn.connect("clicked", self.on_apply)
        btn_box.pack_start(apply_btn, False, False, 0)
        right.pack_start(btn_box, False, False, 0)

        # Başlangıç uyarı kontrolü
        self.check_warning()

    def on_de_selected(self, btn, de_id):
        # Önceki seçimi temizle
        for did, b in self.de_buttons.items():
            ctx = b.get_style_context()
            ctx.remove_class("card-selected")
        # Yenisini işaretle
        self.selected_de = de_id
        self.de_buttons[de_id].get_style_context().add_class("card-selected")
        self.check_warning()

    def check_warning(self):
        ram = self.hw.get("ram_mb", 4096)
        heavy = ["kde", "gnome", "hyprland"]
        very_heavy = ["kde", "gnome"]
        show = False
        msg = ""
        # 512 MB altı ağır seçerse
        if ram <= 512 and self.selected_de not in ["i3wm", "openbox"]:
            show = True
            msg = f"⚠️  Seçtiğiniz <b>{DESKTOPS[self.selected_de]['name']}</b> donanımınız için ağır olabilir (RAM: {ram} MB). Performans sorunları yaşayabilirsiniz. Yine de devam etmek istiyor musunuz?"
        elif ram <= 2048 and self.selected_de in very_heavy:
            show = True
            msg = f"⚠️  <b>{DESKTOPS[self.selected_de]['name']}</b> {ram} MB RAM için ağır olabilir. LXQt veya XFCE daha akıcı çalışır. Yine de devam edilsin mi?"
        elif ram <= 4096 and self.selected_de == "hyprland" and self.hw.get("gpu_type") == "virtual":
            show = True
            msg = "⚠️  Hyprland sanal makinede veya düşük GPU'da sorun çıkarabilir. Wayland desteğini kontrol edin."
        if show:
            self.warning_label.set_markup(f'<span background="#E91E8C" color="white"> {msg} </span>')
            self.warning_box.show_all()
        else:
            self.warning_box.hide()

    def on_apply(self, btn):
        # Seçimi kaydet
        out_paths = ["/tmp/hiroki-selected-de", "/tmp/hiroki-hw-info.json"]
        try:
            with open("/tmp/hiroki-selected-de", "w") as f:
                f.write(self.selected_de + "\n")
            # Ayrıca hw json'u güncelle
            self.hw["selected"] = self.selected_de
            with open(HW_JSON, "w") as f:
                json.dump(self.hw, f, indent=2)
            print(f"[Hiroki] Seçilen DE: {self.selected_de}")

            # Eğer Calamares modu ise, packagechooser seçimini etkilemek için bir flag yaz
            if os.path.exists("/tmp/calamares-hiroki-mode"):
                # Calamares packagechooser için seçimi /tmp'ye yaz
                with open("/tmp/hiroki-calamares-selection.json", "w") as f:
                    json.dump({"selected": self.selected_de}, f)

            # Live ortamda ise: bilgi dialogu
            dialog = Gtk.MessageDialog(
                transient_for=self,
                flags=0,
                message_type=Gtk.MessageType.INFO,
                buttons=Gtk.ButtonsType.OK,
                text=f"{DESKTOPS[self.selected_de]['name']} seçildi!"
            )
            dialog.format_secondary_text(
                f"Hiroki felsefesi: Seçiminize saygı duyuyoruz.\n\n"
                f"Live ortamda masaüstü değişimi için oturumu yeniden başlatmanız veya display manager üzerinden seçim yapmanız gerekebilir.\n"
                f"Calamares kurulumunda bu seçim otomatik olarak uygulanacaktır.\n\n"
                f"Seçim: {self.selected_de}\nDosya: /tmp/hiroki-selected-de"
            )
            dialog.run()
            dialog.destroy()

        except Exception as e:
            err = Gtk.MessageDialog(transient_for=self, flags=0, message_type=Gtk.MessageType.ERROR, buttons=Gtk.ButtonsType.OK, text="Hata")
            err.format_secondary_text(str(e))
            err.run()
            err.destroy()

def main():
    # hw-detect'i her açılışta yenile
    run_hw_detect()
    win = HirokiDESelector()
    win.show_all()
    # Uyarı kutusu başlangıçta gizli kalsın - show_all sonrası tekrar gizle
    win.warning_box.hide()
    Gtk.main()

if __name__ == "__main__":
    main()
