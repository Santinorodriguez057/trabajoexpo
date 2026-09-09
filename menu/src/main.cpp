#include <SDL.h>
#include <SDL_ttf.h>

#include <cstdio>

#include "game_registry.h"
#include "menu.h"

static const char* kRegistryPath = "games.registry";

static TTF_Font* loadFont() {
    // Rutas de fuentes comunes en Linux y Windows.
    const char* candidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
    };
    for (const char* path : candidates) {
        TTF_Font* f = TTF_OpenFont(path, 24);
        if (f) return f;
    }
    return nullptr;
}

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
        std::fprintf(stderr, "SDL_Init fallo: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() < 0) {
        std::fprintf(stderr, "TTF_Init fallo: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window* win = SDL_CreateWindow("Mini Juegos", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, 960, 640, 0);
    if (!win) {
        std::fprintf(stderr, "No se pudo crear la ventana: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) {
        std::fprintf(stderr, "No se pudo crear el renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    TTF_Font* font = loadFont();
    if (!font) {
        std::fprintf(stderr, "Aviso: no se encontro una fuente TTF, el menu se vera vacio.\n");
    }

    GameRegistry registry;
    if (!registry.load(kRegistryPath)) {
        std::fprintf(stderr, "Aviso: no se pudo cargar %s\n", kRegistryPath);
    }

    Menu menu(ren, font, registry.games());
    int result = menu.run();

    if (font) TTF_CloseFont(font);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
    return result;
}