#!/bin/bash
# Сборка: matrix -> ./build/matrix
set -euo pipefail

mkdir -p build
g++ -O2 matrix_theme.cpp -lncurses -o build/matrix
echo "[+] Собрано: build/matrix  (запуск: ./build/matrix)"
