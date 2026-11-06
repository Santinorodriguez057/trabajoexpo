@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ========================================================
echo          DUAL SHOW - INICIANDO SISTEMA ARCADE           
echo ========================================================
echo.

set REBUILD_NEEDED=0

rem 1. Verificar si hay Git instalado y actualizar desde GitHub
where git >nul 2>nul
if %errorlevel% equ 0 (
    if exist ".git" (
        echo [*] Comprobando actualizaciones en GitHub...
        git fetch origin main >nul 2>nul
        if !errorlevel! equ 0 (
            for /f %%i in ('git rev-parse HEAD') do set LOCAL_HASH=%%i
            for /f %%i in ('git rev-parse origin/main') do set REMOTE_HASH=%%i

            if not "!LOCAL_HASH!"=="!REMOTE_HASH!" (
                echo [!] Nueva version detectada. Descargando cambios...
                git pull origin main
                set REBUILD_NEEDED=1
            ) else (
                echo [OK] El juego esta actualizado.
            )
        ) else (
            echo [i] Sin conexion a GitHub o modo offline.
        )
    )
)

rem 2. Compilar si no existe el ejecutable o si hubo una actualizacion
if not exist "build\bin\menu.exe" (
    set REBUILD_NEEDED=1
)

if "!REBUILD_NEEDED!"=="1" (
    where cmake >nul 2>nul
    if !errorlevel! neq 0 (
        echo [ERROR] No se encontro CMake para compilar la actualizacion.
        if exist "build\bin\menu.exe" (
            echo Abriendo version anterior existente...
            goto LAUNCH
        )
        echo Por favor instala CMake para compilar el proyecto en Windows.
        pause
        exit /b 1
    )

    echo [*] Configurando y compilando con CMake...
    cmake -S . -B build
    cmake --build build --config Release
    if !errorlevel! neq 0 (
        echo [ERROR] Fallo la compilacion.
        pause
        exit /b 1
    )
)

:LAUNCH
echo [*] Iniciando Dual Show...
if exist "build\bin\menu.exe" (
    start "" "build\bin\menu.exe"
) else (
    echo [ERROR] No se encontro el ejecutable build\bin\menu.exe
    pause
    exit /b 1
)
