# Hiroki OS - live user bash_profile
# TTY1'de Wayland session otomatik baslat

if [ -z "$WAYLAND_DISPLAY" ] && [ "$XDG_VTNR" = "1" ]; then
    export XDG_SESSION_TYPE=wayland
    export XDG_SESSION_DESKTOP=KDE
    export GDK_DISABLE_SANDBOXED_LOADER=1
    
    # Secili DE'yi oku
    SESSION="kde"
    [ -f /etc/hiroki/selected-de ] && SESSION=$(cat /etc/hiroki/selected-de | tr -d '[:space:]')
    
    case "$SESSION" in
        kde|plasma)  exec startplasma-wayland ;;
        lxqt)        exec startlxqt ;;
        *)           exec startplasma-wayland ;;
    esac
fi
