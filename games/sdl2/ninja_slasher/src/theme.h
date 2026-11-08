#pragma once

#include <SDL.h>

namespace CyberTheme {
    // Paleta Cyber-Neobrutalista
    inline const SDL_Color cDarkBg      = {16, 18, 27, 255};   // Azul/Gris oscuro frío
    inline const SDL_Color cGridLine    = {28, 32, 48, 255};
    inline const SDL_Color cBlack       = {18, 18, 18, 255};   // Borde exterior duro
    inline const SDL_Color cWhite       = {245, 245, 250, 255};

    // Jugadores
    inline const SDL_Color cP1Cyan      = {6, 182, 212, 255};  // Cian Neón (P1)
    inline const SDL_Color cP1Trail     = {34, 211, 238, 255};
    inline const SDL_Color cP2Pink      = {244, 63, 94, 255};  // Rosa/Magenta Eléctrico (P2)
    inline const SDL_Color cP2Trail     = {251, 113, 133, 255};

    // Elementos de juego
    inline const SDL_Color cYellow      = {250, 204, 21, 255}; // Amarillo Cyberpunk (Oro / Energía)
    inline const SDL_Color cGreen       = {34, 197, 94, 255};  // Verde Esmeralda (Perfecto / Salud)
    inline const SDL_Color cRed         = {239, 68, 68, 255};  // Rojo Alerta (Mina PEM / Daño)
    inline const SDL_Color cPurple      = {168, 85, 247, 255}; // Púrpura (Robots)
    inline const SDL_Color cSilver      = {203, 213, 225, 255};
    inline const SDL_Color cOrange      = {251, 146, 60, 255};
    inline const SDL_Color cIceCyan     = {56, 189, 248, 255}; // Celeste hielo (Congelación)
    inline const SDL_Color cNeonFlame   = {255, 94, 0, 255};   // Fuego naranja neón (Frenesí)
}

