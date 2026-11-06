#!/usr/bin/env bash
# ==============================================================================
# Lanzador, Descargador y Auto-Actualizador para Ubuntu / Linux
# ==============================================================================
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

echo "========================================================"
echo "         DUAL SHOW - INICIANDO SISTEMA ARCADE           "
echo "========================================================"

RELEASE_URL="https://github.com/Santinorodriguez057/trabajoexpo/releases/download/latest/DualShow-Linux.tar.gz"

# 1. Comprobar si hay Git y actualizar el repositorio si hay conexión
REBUILD_NEEDED=false

if command -v git >/dev/null 2>&1 && [ -d ".git" ]; then
    echo "[*] Comprobando actualizaciones de codigo en GitHub..."
    if git fetch origin main --timeout=5 >/dev/null 2>&1; then
        LOCAL_HASH=$(git rev-parse HEAD)
        REMOTE_HASH=$(git rev-parse origin/main)

        if [ "$LOCAL_HASH" != "$REMOTE_HASH" ]; then
            echo "[!] Nueva version detectada. Descargando cambios de Git..."
            git pull --ff-only origin main || git pull origin main
            REBUILD_NEEDED=true
        else
            echo "[OK] Repositorio al dia con GitHub."
        fi
    else
        echo "[i] Sin conexion a GitHub o modo offline."
    fi
fi

# 2. Verificar si faltan ejecutables o assets indispensables
MISSING_BIN=false
if [ ! -f "build/bin/menu" ] || [ ! -f "build/bin/carrera_turbo" ] || [ ! -f "build/bin/tiro_con_arco" ] || [ ! -f "build/bin/duelo_vaqueros" ]; then
    MISSING_BIN=true
fi

MISSING_ASSETS=false
if [ ! -d "assets" ] || [ ! -f "assets/menu_bg.bmp" ]; then
    MISSING_ASSETS=true
fi

# 3. Si falta compilar o faltan binarios:
if [ "$REBUILD_NEEDED" = true ] || [ "$MISSING_BIN" = true ] || [ "$MISSING_ASSETS" = true ]; then
    CAN_BUILD=false
    if command -v cmake >/dev/null 2>&1 && command -v g++ >/dev/null 2>&1; then
        CAN_BUILD=true
    fi

    # Intentar compilar si están las herramientas disponibles
    BUILD_SUCCESS=false
    if [ "$CAN_BUILD" = true ] && [ "$MISSING_ASSETS" = false ]; then
        echo "[*] Compilando con CMake..."
        if cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1 && cmake --build build -j"$(nproc)" >/dev/null 2>&1; then
            BUILD_SUCCESS=true
            echo "[OK] Compilacion local completada."
        fi
    fi

    # Si no se pudo compilar o faltan cosas, descargar el paquete precompilado de GitHub Releases
    if [ "$BUILD_SUCCESS" = false ]; then
        echo "[!] Detectados archivos o ejecutables faltantes."
        echo "[*] Descargando version precompilada completa desde GitHub..."

        DOWNLOADED=false
        if command -v curl >/dev/null 2>&1; then
            if curl -L --fail --output /tmp/DualShow-Linux.tar.gz "$RELEASE_URL"; then
                DOWNLOADED=true
            fi
        elif command -v wget >/dev/null 2>&1; then
            if wget -O /tmp/DualShow-Linux.tar.gz "$RELEASE_URL"; then
                DOWNLOADED=true
            fi
        fi

        if [ "$DOWNLOADED" = true ]; then
            echo "[*] Extrayendo paquete precompilado..."
            tar -xzf /tmp/DualShow-Linux.tar.gz -C "$DIR"
            rm -f /tmp/DualShow-Linux.tar.gz
            chmod +x build/bin/* JUGAR.sh 2>/dev/null || true
            echo "[OK] Descarga e instalacion completadas con exito."
        else
            echo "[AVISO] No se pudo descargar la release precompilada (verifique conexion)."
        fi
    fi
fi

# 4. Asegurar permisos de ejecución en los binarios
if [ -d "build/bin" ]; then
    chmod +x build/bin/* 2>/dev/null || true
fi

# 5. Iniciar el juego
if [ -f "build/bin/menu" ]; then
    echo "[*] Iniciando Dual Show..."
    exec ./build/bin/menu "$@"
else
    echo "[ERROR] No se pudo encontrar ni descargar el ejecutable build/bin/menu."
    echo "Presiona Enter para salir..."
    read -r _
    exit 1
fi
