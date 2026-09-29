cmake_minimum_required(VERSION 3.16)
project(MiniGamesMenu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Todos los ejecutables van a build/bin/ (sin subcarpeta Release/Debug)
# para que las rutas de games.registry sean las mismas en Linux y Windows.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
foreach(cfg ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${cfg} cfg_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg_upper} ${CMAKE_BINARY_DIR}/bin)
endforeach()

add_subdirectory(menu)

# ---------------------------------------------------------------------------
# Juegos SDL2: anade aqui cada juego nuevo con add_subdirectory(...)
# Ejemplo:
#   add_subdirectory(games/sdl2/mi_juego)
# ---------------------------------------------------------------------------
add_subdirectory(games/sdl2/template_game)
add_subdirectory(games/sdl2/duelo_vaqueros)
add_subdirectory(games/sdl2/test_mando)