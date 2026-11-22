#!/bin/bash
# Установка: бинарник в /usr/local/bin + (на Omarchy) хук перекраски при смене темы.
# Если есть готовый бинарь (например статический из build-portable.sh) — ставим его,
# иначе определяем пакетный менеджер, ставим зависимости и собираем на месте.
set -euo pipefail

have_cxx() { command -v g++ &>/dev/null || command -v c++ &>/dev/null; }

have_ncurses_h() {
    local d
    for d in /usr/include /usr/local/include /usr/include/*-linux-gnu*; do
        [[ -f "$d/ncurses.h" || -f "$d/ncurses/ncurses.h" ]] && return 0
    done
    return 1
}

install_pkgs() {
    (( $# )) || return 0
    echo "[*] Ставлю зависимости: $*"
    if [[ ${EUID:-$(id -u)} -ne 0 ]] && command -v sudo &>/dev/null; then
        sudo "$@"
    else
        "$@"
    fi
}

# --- Сборка ------------------------------------------------------------------
mkdir -p build
if [[ -f build/matrix ]]; then
    echo "[*] Использую готовый бинарь build/matrix"
elif command -v docker &>/dev/null; then
    echo "[*] Docker найден — собираю статический бинарь (работает на любом дистрибутиве)"
    ./build-portable.sh
else
    # Зависимости для локальной сборки
    if command -v pacman &>/dev/null; then
        pkgs=()
        pacman -T ncurses &>/dev/null || pkgs+=(ncurses)
        have_cxx || pkgs+=(gcc)
        ((${#pkgs[@]})) && install_pkgs pacman -S --needed "${pkgs[@]}" || true
    elif command -v apt-get &>/dev/null; then
        install_pkgs apt-get install -y build-essential libncurses-dev
    elif command -v dnf &>/dev/null; then
        pkgs=()
        have_cxx || pkgs+=(gcc-c++)
        have_ncurses_h || pkgs+=(ncurses-devel)
        ((${#pkgs[@]})) && install_pkgs dnf install -y "${pkgs[@]}" || true
    elif command -v zypper &>/dev/null; then
        pkgs=()
        have_cxx || pkgs+=(gcc-c++)
        have_ncurses_h || pkgs+=(ncurses-devel)
        ((${#pkgs[@]})) && install_pkgs zypper --non-interactive install "${pkgs[@]}" || true
    elif command -v apk &>/dev/null; then
        pkgs=()
        have_cxx || pkgs+=(g++)
        have_ncurses_h || pkgs+=(ncurses-dev)
        ((${#pkgs[@]})) && install_pkgs apk add "${pkgs[@]}" || true
    elif command -v xbps-install &>/dev/null; then
        pkgs=()
        have_cxx || pkgs+=(gcc)
        have_ncurses_h || pkgs+=(ncurses-devel)
        ((${#pkgs[@]})) && install_pkgs xbps-install -Sy "${pkgs[@]}" || true
    elif command -v nix-shell &>/dev/null && ! have_cxx && ! have_ncurses_h; then
        echo "[*] NixOS: собираю в окружении nix-shell (g++ + ncurses)"
    else
        echo "[!] Не найден известный пакетный менеджер."
        echo "    Поставь руками: g++ и ncurses (dev-пакет с заголовками), затем ./build.sh"
        echo "    Примеры: apt-get install build-essential libncurses-dev"
        echo "             dnf install gcc-c++ ncurses-devel"
        echo "             pacman -S gcc ncurses"
        exit 1
    fi

    if [[ ! -f build/matrix ]]; then
        if command -v nix-shell &>/dev/null && ! have_cxx; then
            nix-shell -p gcc ncurses --run "g++ -O2 -static-libstdc++ -static-libgcc matrix_theme.cpp -lncursesw -ltinfo -o build/matrix"
        else
            g++ -O2 -static-libstdc++ -static-libgcc matrix_theme.cpp -lncursesw -ltinfo -o build/matrix
        fi
    fi
fi

# --- Установка бинаря --------------------------------------------------------
if [[ ${EUID:-$(id -u)} -eq 0 ]] || ! command -v sudo &>/dev/null; then
    install -m 755 build/matrix /usr/local/bin/matrix
else
    sudo install -m 755 build/matrix /usr/local/bin/matrix
fi

# --- Хук Omarchy (только если это Omarchy) ----------------------------------
if [[ -d "$HOME/.config/omarchy" ]]; then
    HOOK_DIR="$HOME/.config/omarchy/hooks/theme-set.d"
    mkdir -p "$HOOK_DIR"
    install -m 755 90-matrix-reload "$HOOK_DIR/90-matrix-reload"
fi

echo "[+] Установлено. Пиши: matrix  (выход: q)"
