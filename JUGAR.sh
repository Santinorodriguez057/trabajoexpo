#!/usr/bin/env bash
# ==============================================================================
# Lanzador y Auto-Actualizador para Ubuntu / Linux
# ==============================================================================
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

echo "========================================================"
echo "         DUAL SHOW - INICIANDO SISTEMA ARCADE           "
echo "========================================================"

# 1. Comprobar si hay Git y actualizar con origin/main si hay conexión
REBUILD_NEEDED=false

if command -v git >/dev/null 2>&1 && [ -d ".git" ]; then
    echo "[*] Comprobando actualizaciones en GitHub..."
    if git fetch origin main --timeout=5 >/dev/null 2>&1; then
        LOCAL_HASH=$(git rev-parse HEAD)
        REMOTE_HASH=$(git rev-parse origin/main)

        if [ "$LOCAL_HASH" != "$REMOTE_HASH" ]; then
            echo "[!] Nueva version detectada. Actualizando repositorio..."
            git pull --ff-only origin main || git pull origin main
            REBUILD_NEEDED=true
        else
            echo "[OK] El proyecto ya esta en la version mas reciente."
        fi
    else
        echo "[i] Sin conexion a GitHub o modo offline. Continuando..."
    fi
fi

# 2. Compilar si hubo cambios o si no existe el binario del menu
if [ ! -f "build/bin/menu" ] || [ "$REBUILD_NEEDED" = true ]; then
    echo "[*] Compilando juego..."
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1 || cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j"$(nproc)"
    echo "[OK] Compilacion completada con exito."
fi

# 3. Lanzar el menu principal
echo "[*] Abriendo Dual Show..."
exec ./build/bin/menu "$@"
