#!/usr/bin/env bash
# Hiroki OS - Tema Uygulama Scripti
# Seçilen DE'ye göre Hiroki temasını uygular
set -euo pipefail
DE="${1:-xfce}"
echo "[Hiroki Tema] DE: $DE için tema uygulanıyor"

# Genel GTK tema ayarları için skel
SKEL="/etc/skel"
mkdir -p "$SKEL/.config/gtk-3.0" "$SKEL/.config/gtk-4.0" 2>/dev/null || true

# GTK ayarları
cat > "$SKEL/.config/gtk-3.0/settings.ini" <<'EOS'
[Settings]
gtk-theme-name=Hiroki-Dark
gtk-icon-theme-name=Hiroki-Icons
gtk-font-name=Inter 10
gtk-cursor-theme-name=Bibata-Modern-Classic
gtk-cursor-theme-size=24
gtk-toolbar-style=GTK_TOOLBAR_BOTH_HORIZ
gtk-toolbar-icon-size=GTK_ICON_SIZE_LARGE_TOOLBAR
gtk-button-images=1
gtk-menu-images=1
gtk-enable-event-sounds=1
gtk-enable-input-feedback-sounds=1
gtk-xft-antialias=1
gtk-xft-hinting=1
gtk-xft-hintstyle=hintfull
EOS

cat > "$SKEL/.config/gtk-3.0/gtk.css" <<'EOS'
/* Hiroki OS - Kullanıcı GTK özelleştirmeleri */
@define-color hiroki_primary #2D1B69;
@define-color hiroki_pink #E91E8C;
@define-color hiroki_turquoise #00D4AA;
@define-color hiroki_bg_dark #0D0D1A;
@define-color hiroki_bg #1A1A2E;
EOS

# XFCE özel
if [[ "$DE" == "xfce" ]]; then
    mkdir -p "$SKEL/.config/xfce4/xfconf/xfce-perchannel-xml" 2>/dev/null || true
    # xsettings
    cat > "$SKEL/.config/xfce4/xfconf/xfce-perchannel-xml/xsettings.xml" <<'EOS'
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xsettings" version="1.0">
  <property name="Net" type="empty">
    <property name="ThemeName" type="string" value="Hiroki-Dark"/>
    <property name="IconThemeName" type="string" value="Hiroki-Icons"/>
    <property name="CursorThemeName" type="string" value="Bibata-Modern-Classic"/>
    <property name="CursorThemeSize" type="int" value="24"/>
  </property>
  <property name="Gtk" type="empty">
    <property name="FontName" type="string" value="Inter 10"/>
    <property name="MonospaceFontName" type="string" value="Fira Code 10"/>
  </property>
</channel>
EOS
    # xfce4-panel
    cat > "$SKEL/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-panel.xml" <<'EOS'
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xfce4-panel" version="1.0">
  <property name="panels" type="array">
    <value type="int" value="1"/>
    <property name="panel-1" type="empty">
      <property name="position" type="string" value="p=6;x=0;y=0"/>
      <property name="length" type="uint" value="100"/>
      <property name="position-locked" type="bool" value="true"/>
      <property name="size" type="uint" value="30"/>
      <property name="background-style" type="uint" value="1"/>
      <property name="background-rgba" type="array">
        <value type="double" value="0.10196"/>
        <value type="double" value="0.10196"/>
        <value type="double" value="0.18039"/>
        <value type="double" value="0.85"/>
      </property>
    </property>
  </property>
</channel>
EOS
    # Whisker menu - Hiroki logosu (kopyalanacak ikon)
    mkdir -p "$SKEL/.config/xfce4/panel" 2>/dev/null || true
fi

# KDE Plasma özel
if [[ "$DE" == "kde" ]]; then
    mkdir -p "$SKEL/.config" 2>/dev/null || true
    cat > "$SKEL/.config/kdeglobals" <<'EOS'
[General]
ColorScheme=HirokiDark
Name=Hiroki Dark
XftHintStyle=hintfull
XftSubPixel=rgb

[Icons]
Theme=Hiroki-Icons

[WM]
activeBackground=45,27,105
activeForeground=234,234,234
inactiveBackground=26,26,46
inactiveForeground=160,160,184
EOS
    cat > "$SKEL/.config/kwinrc" <<'EOS'
[Compositing]
Enabled=true
Backend=OpenGL

[Desktops]
Number=2
Rows=1

[Windows]
BorderSize=Normal
EOS
fi

# GNOME özel
if [[ "$DE" == "gnome" ]]; then
    # dconf ayarları için script oluştur (ilk girişte çalışır)
    mkdir -p "$SKEL/.config/autostart" 2>/dev/null || true
    cat > "$SKEL/.config/autostart/hiroki-gnome-setup.desktop" <<'EOS'
[Desktop Entry]
Type=Application
Name=Hiroki GNOME Setup
Exec=/usr/bin/hiroki-gnome-setup
Hidden=false
NoDisplay=false
X-GNOME-Autostart-enabled=true
EOS
    cat > /usr/bin/hiroki-gnome-setup <<'EOS2'
#!/usr/bin/env bash
gsettings set org.gnome.desktop.interface gtk-theme 'Hiroki-Dark' 2>/dev/null || true
gsettings set org.gnome.desktop.interface icon-theme 'Hiroki-Icons' 2>/dev/null || true
gsettings set org.gnome.desktop.interface cursor-theme 'Bibata-Modern-Classic' 2>/dev/null || true
gsettings set org.gnome.desktop.background picture-uri 'file:///usr/share/hiroki/wallpapers/sakura-gradient.jpg' 2>/dev/null || true
gsettings set org.gnome.desktop.background picture-uri-dark 'file:///usr/share/hiroki/wallpapers/sakura-gradient.jpg' 2>/dev/null || true
# Extensions
gsettings set org.gnome.shell enabled-extensions "['dash-to-dock@micxgx.gmail.com', 'appindicatorsupport@rgcjonas.gmail.com', 'user-theme@gnome-shell-extensions.gcampax.github.com']" 2>/dev/null || true
rm -f ~/.config/autostart/hiroki-gnome-setup.desktop
EOS2
    chmod +x /usr/bin/hiroki-gnome-setup 2>/dev/null || true
fi

# i3wm özel
if [[ "$DE" == "i3wm" ]]; then
    mkdir -p "$SKEL/.config/i3" "$SKEL/.config/polybar" "$SKEL/.config/rofi" "$SKEL/.config/dunst" 2>/dev/null || true
    cat > "$SKEL/.config/i3/config" <<'EOS'
# Hiroki OS - i3wm Config - Sakura
# Renkler: #2D1B69 mor, #E91E8C pembe, #00D4AA turkuaz, #0D0D1A koyu, #1A1A2E lacivert
set $mod Mod4
set $hiroki_bg #0D0D1A
set $hiroki_bg2 #1A1A2E
set $hiroki_primary #2D1B69
set $hiroki_pink #E91E8C
set $hiroki_turq #00D4AA
set $hiroki_text #EAEAEA
set $hiroki_gray #A0A0B8

font pango:Inter 10

# Hiroki renk paleti
client.focused          $hiroki_pink $hiroki_primary $hiroki_text $hiroki_pink $hiroki_pink
client.focused_inactive $hiroki_bg2 $hiroki_bg2 $hiroki_gray $hiroki_bg2 $hiroki_bg2
client.unfocused        $hiroki_bg2 $hiroki_bg2 $hiroki_gray $hiroki_bg2 $hiroki_bg2
client.urgent           $hiroki_pink $hiroki_pink $hiroki_text $hiroki_pink $hiroki_pink
client.placeholder      $hiroki_bg $hiroki_bg $hiroki_text $hiroki_bg $hiroki_bg
client.background       $hiroki_bg

# Gaps ve border
gaps inner 8
gaps outer 4
smart_gaps on
default_border pixel 2
default_floating_border pixel 2
hide_edge_borders smart

# Otostart
exec --no-startup-id picom --experimental-backends &
exec --no-startup-id dunst &
exec --no-startup-id nm-applet &
exec --no-startup-id /usr/lib/polkit-gnome/polkit-gnome-authentication-agent-1 &
exec --no-startup-id feh --bg-scale /usr/share/hiroki/wallpapers/sakura-gradient.jpg

# Kısayollar
bindsym $mod+Return exec alacritty
bindsym $mod+d exec --no-startup-id rofi -show drun -theme /usr/share/rofi/themes/hiroki.rasi
bindsym $mod+Shift+q kill
bindsym $mod+Shift+e exec "i3-nagbar -t warning -m 'Çıkmak istiyor musun?' -B 'Evet, çık' 'i3-msg exit'"

# Hiroki rehber
bindsym $mod+F1 exec --no-startup-id alacritty -e sh -c 'cat /usr/share/hiroki/i3-shortcuts.txt; read'

# Çalışma alanları
set $ws1 "1"
set $ws2 "2"
set $ws3 "3"
set $ws4 "4"
set $ws5 "5"
bindsym $mod+1 workspace number $ws1
bindsym $mod+2 workspace number $ws2
bindsym $mod+3 workspace number $ws3
bindsym $mod+4 workspace number $ws4
bindsym $mod+5 workspace number $ws5

# Bar - polybar tercihli, yoksa i3bar
bar {
    status_command i3status
    position top
    colors {
        background $hiroki_bg
        statusline $hiroki_text
        separator $hiroki_gray
        focused_workspace  $hiroki_pink $hiroki_pink $hiroki_text
        active_workspace   $hiroki_primary $hiroki_primary $hiroki_text
        inactive_workspace $hiroki_bg2 $hiroki_bg2 $hiroki_gray
        urgent_workspace   $hiroki_pink $hiroki_pink $hiroki_text
    }
}
EOS
    cat > "$SKEL/.config/i3/i3-shortcuts.txt" <<'EOS'
Hiroki OS i3wm Kısayolları:
Mod = Super (Windows) tuşu
Mod+Enter = Terminal (Alacritty)
Mod+d = Uygulama başlatıcı (Rofi)
Mod+Shift+q = Pencereyi kapat
Mod+1..5 = Çalışma alanı değiştir
Mod+Shift+e = Çıkış
Mod+F1 = Bu yardım
EOS
fi

# Hyprland özel
if [[ "$DE" == "hyprland" ]]; then
    mkdir -p "$SKEL/.config/hypr" "$SKEL/.config/waybar" 2>/dev/null || true
    cat > "$SKEL/.config/hypr/hyprland.conf" <<'EOS'
# Hiroki OS - Hyprland Config - Sakura
# Renkler Hiroki paleti
general {
    gaps_in = 6
    gaps_out = 10
    border_size = 2
    col.active_border = rgba(E91E8Cff) rgba(2D1B69ff) 45deg
    col.inactive_border = rgba(1A1A2Eff) rgba(0D0D1Aff) 45deg
    layout = dwindle
}
decoration {
    rounding = 12
    blur {
        enabled = true
        size = 6
        passes = 3
    }
    drop_shadow = true
    shadow_range = 12
    shadow_render_power = 3
    col.shadow = rgba(0D0D1A99)
}
animations {
    enabled = true
    bezier = hiroki, 0.05, 0.9, 0.1, 1.05
    animation = windows, 1, 5, hiroki, slide
    animation = border, 1, 8, default
    animation = fade, 1, 5, default
    animation = workspaces, 1, 6, default
}
input {
    kb_layout = tr
    follow_mouse = 1
    touchpad { natural_scroll = true }
}
$mainMod = SUPER
bind = $mainMod, Return, exec, kitty
bind = $mainMod, D, exec, wofi --show drun -c ~/.config/wofi/hiroki.css
bind = $mainMod, Q, killactive,
bind = $mainMod SHIFT, E, exit,
bind = $mainMod, F, fullscreen,

exec-once = waybar
exec-once = hyprpaper
exec-once = mako
exec-once = wl-paste --watch cliphist store
EOS
fi

# Openbox özel
if [[ "$DE" == "openbox" ]]; then
    mkdir -p "$SKEL/.config/openbox" "$SKEL/.config/tint2" 2>/dev/null || true
    cat > "$SKEL/.config/openbox/rc.xml" <<'EOS'
<?xml version="1.0" encoding="UTF-8"?>
<openbox_config xmlns="http://openbox.org/3.4/rc">
  <theme><name>Hiroki-Dark</name><titleLayout>NLIMC</titleLayout></theme>
  <desktops><number>4</number></desktops>
</openbox_config>
EOS
fi

echo "[Hiroki Tema] Tema uygulandı: $DE"
