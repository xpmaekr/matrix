#!/bin/bash
# Установка: бинарник в /usr/local/bin + хук перекраски при смене темы Omarchy
set -euo pipefail

# Зависимости: ncurses и gcc — ставим через pacman, если их нет.
pkgs=()
pacman -T ncurses &>/dev/null || pkgs+=(ncurses)
command -v g++ &>/dev/null || pkgs+=(gcc)
if ((${#pkgs[@]})); then
    echo "[*] Ставлю зависимости: ${pkgs[*]}"
    sudo pacman -S --needed "${pkgs[@]}"
fi

if [[ ! -f build/matrix ]]; then
    ./build.sh
fi

sudo install -m 755 build/matrix /usr/local/bin/matrix

HOOK_DIR="$HOME/.config/omarchy/hooks/theme-set.d"
mkdir -p "$HOOK_DIR"
install -m 755 90-matrix-reload "$HOOK_DIR/90-matrix-reload"

echo "[+] Установлено. Пиши: matrix  (выход: q)"
