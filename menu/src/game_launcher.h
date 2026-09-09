#pragma once

#include <string>

// Lanza un juego como proceso hijo y espera a que termine.
class GameLauncher {
public:
    // Devuelve el codigo de salida del juego, o -1 si no se pudo lanzar.
    int launch(const std::string& executable);
};