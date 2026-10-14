@echo off
setlocal
cd /d "%~dp0.."

echo ========================================================
echo   Compilando Trabajo Expo (Menu + Juegos SDL2)
echo ========================================================
echo.

rem Verificar si cmake esta disponible
where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] No se encontro 'cmake' en el sistema o en el PATH.
    echo Por favor instala CMake desde https://cmake.org/download/
    echo y asegurate de marcar "Add CMake to system PATH".
    echo.
    pause
    exit /b 1
)

echo [1/2] Configurando el proyecto con CMake...
echo (Si es la primera vez, se descargaran las librerias SDL2 automaticamente)
echo.
cmake -S . -B build
if errorlevel 1 (
    echo.
    echo [ERROR] Fallo la configuracion con CMake.
    echo Asegurate de tener instalado un compilador de C++:
    echo  - Visual Studio 2019/2022 con "Desarrollo para el escritorio con C++"
    echo  - o MinGW-w64 (GCC)
    echo.
    pause
    exit /b 1
)

echo.
echo [2/2] Compilando ejecutables en Release...
echo.
cmake --build build --config Release
if errorlevel 1 (
    echo.
    echo [ERROR] Fallo la compilacion.
    echo.
    pause
    exit /b 1
)

echo.
echo ========================================================
echo   Compilacion exitosa!
echo.
echo   Para ejecutar el menu:
echo     build\bin\menu.exe
echo.
echo   O ejecuta individualmente:
echo     build\bin\duelo_vaqueros.exe
echo     build\bin\test_mando.exe
echo     build\bin\tiro_con_arco.exe
echo ========================================================
echo.
pause