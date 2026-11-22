#!/bin/bash
# Портативная сборка через Docker (Alpine/musl): полностью статический бинарь,
# работает на любом Linux x86_64 — glibc, musl, любые версии библиотек.
set -euo pipefail

command -v docker &>/dev/null || { echo "[!] Нужен docker"; exit 1; }

mkdir -p build
docker run --rm \
    -v "$PWD/matrix_theme.cpp:/src/matrix_theme.cpp:ro" \
    -v "$PWD/build:/out" \
    alpine:latest sh -c '
        apk add -q g++ musl-dev ncurses-dev ncurses-static >/dev/null &&
        g++ -O2 -static -D_GNU_SOURCE /src/matrix_theme.cpp -lncursesw -ltinfo -lm -o /out/matrix &&
        strip /out/matrix'
echo "[+] Готово: build/matrix — статический, положи его куда угодно (например в релиз на GitHub)"
