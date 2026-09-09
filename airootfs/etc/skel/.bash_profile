# Hiroki OS - TTY login'den sonra XFCE baslat
# Dikkat: hiroki-dm (display manager) odakli oturumlarda (HIROKI_DM_SESSION=1)
# DISPLAY zaten ayarlanir; burada tekrar X baslatilmaz.
if [ -z "$DISPLAY" ] && [ -z "$HIROKI_DM_SESSION" ] && [ "$(tty)" = "/dev/tty1" ]; then
    exec startx
fi
