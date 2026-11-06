@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ========================================================
echo          DUAL SHOW - INICIANDO SISTEMA ARCADE           
echo ========================================================
echo.

set RELEASE_URL=https://github.com/Santinorodriguez057/trabajoexpo/releases/download/latest/DualShow-Windows.zip
set REBUILD_NEEDED=0
set MISSING_FILES=0

rem 1. Verificar si hay Git instalado y actualizar desde GitHub
where git >nul 2>nul
if %errorlevel% equ 0 (
    if exist ".git" (
        echo [*] Comprobando actualizaciones de codigo en GitHub...
        git fetch origin main >nul 2>nul
        if !errorlevel% equ 0 (
            for /f %%i in ('git rev-parse HEAD') do set LOCAL_HASH=%%i
            for /f %%i in ('git rev-parse origin/main') do set REMOTE_HASH=%%i

            if not "!LOCAL_HASH!"=="!REMOTE_HASH!" (
                echo [!] Nueva version detectada. Descargando cambios de Git...
                git pull origin main
                set REBUILD_NEEDED=1
            ) else (
                echo [OK] Repositorio al dia con GitHub.
            )
        ) else (
            echo [i] Sin conexion a GitHub o modo offline.
        )
    )
)

rem 2. Verificar si faltan ejecutables esenciales o carpetas de assets
if not exist "build\bin\menu.exe" set MISSING_FILES=1
if not exist "build\bin\carrera_turbo.exe" set MISSING_FILES=1
if not exist "build\bin\tiro_con_arco.exe" set MISSING_FILES=1
if not exist "build\bin\duelo_vaqueros.exe" set MISSING_FILES=1
if not exist "assets" set MISSING_FILES=1

rem 3. Si falta compilar o faltan ejecutables/librerias:
if "!REBUILD_NEEDED!"=="1" goto ATTEMPT_BUILD
if "!MISSING_FILES!"=="1" goto ATTEMPT_BUILD
goto LAUNCH

:ATTEMPT_BUILD
set BUILD_SUCCESS=0
where cmake >nul 2>nul
if %errorlevel% equ 0 (
    echo [*] Compilador detectado. Compilando con CMake...
    cmake -S . -B build >nul 2>nul
    cmake --build build --config Release >nul 2>nul
    if exist "build\bin\menu.exe" (
        set BUILD_SUCCESS=1
        echo [OK] Compilacion local completada.
    )
)

if "!BUILD_SUCCESS!"=="1" goto LAUNCH

rem Si no se pudo compilar localmente o faltaban archivos, descargar el paquete completo
echo.
echo [!] Faltan ejecutables o herramientas de desarrollo en esta maquina.
echo [*] Descargando version precompilada completa de Windows desde GitHub...

set TEMP_ZIP=%TEMP%\DualShow-Windows.zip
if exist "!TEMP_ZIP!" del /f /q "!TEMP_ZIP!"

rem Descargar con curl (incluido por defecto en Windows 10/11) o PowerShell
where curl >nul 2>nul
if %errorlevel% equ 0 (
    curl -L --fail -o "!TEMP_ZIP!" "%RELEASE_URL%"
) else (
    powershell -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; (New-Object Net.WebClient).DownloadFile('%RELEASE_URL%', '%TEMP_ZIP%')"
)

if exist "!TEMP_ZIP!" (
    echo [*] Extrayendo archivos precompilados y librerias...
    tar -xf "!TEMP_ZIP!" -C "%~dp0" >nul 2>nul
    if !errorlevel! neq 0 (
        powershell -Command "Expand-Archive -Path '%TEMP_ZIP%' -DestinationPath '%~dp0' -Force"
    )
    del /f /q "!TEMP_ZIP!" >nul 2>nul
    echo [OK] Descarga e instalacion completadas con exito.
) else (
    echo [AVISO] No se pudo descargar la release automatica (verifique su conexion).
)

:LAUNCH
echo.
echo [*] Iniciando Dual Show...
if exist "build\bin\menu.exe" (
    start "" "build\bin\menu.exe"
) else if exist "bin\menu.exe" (
    start "" "bin\menu.exe"
) else (
    echo [ERROR] No se pudo encontrar ni descargar el ejecutable del menu.
    pause
    exit /b 1
)
