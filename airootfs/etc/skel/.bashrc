# Hiroki OS - .bashrc
# Sakura 1.0 - Hiroki renkleri ve karşılama

# Eğer interaktif değilse çık
[[ $- != *i* ]] && return

# Renkli prompt - Hiroki mor (#2D1B69) ve pembe (#E91E8C)
PS1='\[\033[1;35m\]🌸 \[\033[1;34m\]\u@\h\[\033[0m\] \[\033[1;30m\]\w\[\033[0m\] \[\033[1;35m\]❯\[\033[0m\] '

# Aliaslar
alias ls='ls --color=auto'
alias ll='ls -lah'
alias la='ls -A'
alias l='ls -CF'
alias grep='grep --color=auto'
alias pacman='sudo pacman'
alias yay='yay --color=always'
alias update='sudo pacman -Syu'
alias htop='htop --tree'

# Hiroki komutları
alias hiroki-info='cat /etc/os-release && echo "---" && /usr/bin/hiroki-hw-detect'
alias hiroki-theme='hiroki-theme-manager'
alias hiroki-welcome='hiroki-welcome --force'

# Hiroki sistem bilgisi - ilk terminalde PNG logo + sistem bilgisi
if [[ -z "$HIROKI_BASH_DONE" ]]; then
    export HIROKI_BASH_DONE=1
    # Hosgeldin uygulaması - sadece ilk acilista
    if [[ -f /usr/bin/hiroki-welcome ]] && [[ ! -f ~/.hiroki-welcome-done ]]; then
        hiroki-welcome
        touch ~/.hiroki-welcome-done
    fi
fi

# History
HISTSIZE=10000
HISTFILESIZE=20000
HISTCONTROL=ignoreboth
shopt -s histappend
