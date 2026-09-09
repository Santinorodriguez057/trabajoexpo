#!/usr/bin/env bash
# Compila el menu y todos los juegos SDL2.
set -euo pipefail
cd "$(dirname "$0")/.."

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

echo ""
echo "Listo. Ejecuta el menu con:"
echo "  ./build/bin/menu"