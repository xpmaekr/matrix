#!/bin/bash
# Сборка: matrix -> ./build/matrix
set -euo pipefail

mkdir -p build
g++ -O2 -static-libstdc++ -static-libgcc matrix_theme.cpp -lncursesw -ltinfo -o build/matrix
echo "[+] Собрано: build/matrix  (запуск: ./build/matrix)"
