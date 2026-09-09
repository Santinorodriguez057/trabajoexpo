@echo off
rem Compila el menu y todos los juegos SDL2 (Windows).
cd /d "%~dp0.."

cmake -S . -B build
if errorlevel 1 exit /b 1
cmake --build build --config Release
if errorlevel 1 exit /b 1

echo.
echo Listo. Ejecuta el menu con:
echo   build\bin\menu.exe