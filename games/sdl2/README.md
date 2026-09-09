# Juegos SDL2 (C++)

Cada juego es un ejecutable independiente en su propia carpeta:

    games/sdl2/<nombre-del-juego>/
        CMakeLists.txt
        src/main.cpp

## Crear un juego nuevo

1. Copia la carpeta `template_game` y renombrala con el nombre de tu juego.
2. Edita `CMakeLists.txt` (nombre del ejecutable) y `src/main.cpp`.
3. Anade tu juego al `CMakeLists.txt` raiz:

       add_subdirectory(games/sdl2/<nombre-del-juego>)

4. Registralo en `games.registry` (campos separados por TAB):

       <id>	<Nombre visible>	<Descripcion>	build/bin/<id>	build/bin/<id>.exe

## Mandos PS4 en SDL2

- Inicializa con `SDL_INIT_GAMECONTROLLER`.
- Abre el mando con `SDL_GameControllerOpen(0)`.
- Botones: `SDL_CONTROLLER_BUTTON_A` = X, `SDL_CONTROLLER_BUTTON_B` = Circulo, etc.
- IMPORTANTE: cierra el mando (`SDL_GameControllerClose`) antes de salir.

## Regla importante

El juego debe terminar limpiamente (devolver 0) para que el menu
recupere el control. Usa Circulo (B) o ESC para salir.