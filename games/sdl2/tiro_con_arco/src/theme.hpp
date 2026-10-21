#pragma once
#include <SDL.h>
#include <string>
#include <vector>

namespace ArcheryTheme {
    // =========================================================================
    // PALETA DE COLORES NEO-BRUTALISTA (Basada en Duelo de Vaqueros)
    // =========================================================================
    constexpr SDL_Color COLOR_BG           = {245, 245, 245, 255}; // #F5F5F5 Cielo base
    constexpr SDL_Color COLOR_GRID         = {0, 0, 0, 8};         // Cuadrícula animada de fondo
    constexpr SDL_Color COLOR_GROUND       = {230, 230, 230, 255}; // #E6E6E6 Suelo/Llanura
    constexpr SDL_Color COLOR_HORIZON      = {18, 18, 18, 255};    // Línea de horizonte negra pura
    constexpr SDL_Color COLOR_HILLS        = {215, 215, 215, 255}; // #D7D7D7 Siluetas de colinas/mesas
    constexpr SDL_Color COLOR_SUN          = {220, 220, 220, 255}; // Sol geométrico
    constexpr SDL_Color COLOR_CLOUD        = {235, 235, 235, 255}; // Nubes rectangulares
    constexpr SDL_Color COLOR_CARD_BG      = {250, 250, 250, 255}; // Superficie de tarjetas HUD
    constexpr SDL_Color COLOR_CARD_MUTED   = {240, 240, 242, 255}; // Fondo secundario de badges
    constexpr SDL_Color COLOR_SHADOW       = {10, 10, 10, 255};    // Sombra dura sin difuminar
    constexpr SDL_Color COLOR_BORDER       = {15, 15, 15, 255};    // Borde negro brutalista
    constexpr SDL_Color COLOR_TEXT_MAIN    = {15, 15, 15, 255};    // Texto principal
    constexpr SDL_Color COLOR_TEXT_MUTED   = {90, 90, 90, 255};    // Texto secundario / atajos
    constexpr SDL_Color COLOR_ACCENT_GOLD  = {245, 205, 45, 255};  // Acento dorado / sol / tensión
    constexpr SDL_Color COLOR_ACCENT_RED   = {210, 45, 45, 255};   // Acento rojo / récord / alerta
    constexpr SDL_Color COLOR_ACCENT_GREEN = {35, 160, 60, 255};   // Acento verde / listo / éxito
    constexpr SDL_Color COLOR_ACCENT_BLUE  = {30, 95, 185, 255};   // Acento azul frío
    
    // Materiales del Arco y Flecha
    constexpr SDL_Color COLOR_WOOD_DARK    = {115, 60, 25, 255};   // Madera noble brazos del arco
    constexpr SDL_Color COLOR_WOOD_LIGHT   = {160, 95, 45, 255};   // Borde madera
    constexpr SDL_Color COLOR_BOW_GRIP     = {30, 30, 35, 255};    // Empuñadura central
    constexpr SDL_Color COLOR_STRING       = {255, 255, 255, 255}; // Cuerda tensada
    constexpr SDL_Color COLOR_ARROW_SHAFT  = {215, 185, 130, 255}; // Vástago de la flecha
    constexpr SDL_Color COLOR_ARROW_TIP    = {230, 230, 240, 255}; // Punta de acero
    constexpr SDL_Color COLOR_ARROW_FLETCH = {220, 40, 40, 255};   // Plumas rojas

    // =========================================================================
    // DIMENSIONES Y GEOMETRÍA
    // =========================================================================
    constexpr int HORIZON_Y        = 540; // Altura de horizonte en 1920x1080
    constexpr int HORIZON_THICK    = 4;   // Grosor de la línea del horizonte
    constexpr int BORDER_WIDTH     = 4;   // Grosor del borde de cajas
    constexpr int SHADOW_OFFSET    = 12;  // Desplazamiento de sombra dura

    // =========================================================================
    // CONSTANTES DE EXTRAS (Paso 5)
    // Cada extra es activable/desactivable independientemente
    // =========================================================================
    constexpr bool EXTRA_ENABLE_RUMBLE         = true; // Vibración al tensar y soltar
    constexpr bool EXTRA_ENABLE_FLOATING_TEXTS = true; // Texto flotante con flecha y temblor
    constexpr bool EXTRA_ENABLE_WIND           = true; // Viento en el HUD que desvía el tiro
    constexpr bool EXTRA_ENABLE_SAVED_RECORD   = true; // Guardado de récord en archivo
    constexpr bool EXTRA_ENABLE_SUMMARY_SCREEN = true; // Pantalla de resumen tras 10 rondas

    // =========================================================================
    // HELPERS DE RENDERIZADO NEO-BRUTALISTA
    // =========================================================================
    inline void drawBrutalistBox(SDL_Renderer* ren, int x, int y, int w, int h,
                                 bool filledDark = false,
                                 SDL_Color fillCol = COLOR_CARD_BG,
                                 SDL_Color accentBar = {0,0,0,0},
                                 int shadowOffset = SHADOW_OFFSET) {
        // 1. Sombra sólida desplazada
        SDL_SetRenderDrawColor(ren, COLOR_SHADOW.r, COLOR_SHADOW.g, COLOR_SHADOW.b, 255);
        SDL_Rect sR = { x + shadowOffset, y + shadowOffset, w, h };
        SDL_RenderFillRect(ren, &sR);

        // 2. Fondo interior
        SDL_Color bg = filledDark ? COLOR_SHADOW : fillCol;
        SDL_SetRenderDrawColor(ren, bg.r, bg.g, bg.b, 255);
        SDL_Rect bR = { x, y, w, h };
        SDL_RenderFillRect(ren, &bR);

        // 3. Borde exterior negro grueso
        SDL_SetRenderDrawColor(ren, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
        for (int b = 0; b < BORDER_WIDTH; ++b) {
            SDL_Rect bor = { x + b, y + b, w - b * 2, h - b * 2 };
            SDL_RenderDrawRect(ren, &bor);
        }

        // 4. Franja superior de acento
        if (accentBar.a > 0) {
            SDL_SetRenderDrawColor(ren, accentBar.r, accentBar.g, accentBar.b, accentBar.a);
            SDL_Rect bar = { x + BORDER_WIDTH, y + BORDER_WIDTH, w - BORDER_WIDTH * 2, 6 };
            SDL_RenderFillRect(ren, &bar);
        }
    }
}
