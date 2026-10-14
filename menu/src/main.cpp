#include <SDL.h>
#include <SDL_ttf.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif

#include "game_registry.h"
#include "menu.h"

static const char* kRegistryPath = "games.registry";

static TTF_Font* loadFont() {
    std::vector<std::string> candidates = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    };

    const char* winDir = std::getenv("WINDIR");
    if (!winDir) winDir = std::getenv("SystemRoot");
    std::string winBase = winDir ? winDir : "C:\\Windows";
    candidates.push_back(winBase + "\\Fonts\\arial.ttf");
    candidates.push_back(winBase + "\\Fonts\\segoeui.ttf");
    candidates.push_back(winBase + "\\Fonts\\tahoma.ttf");
    candidates.push_back(winBase + "\\Fonts\\calibri.ttf");
    candidates.push_back("C:\\Windows\\Fonts\\arial.ttf");
    candidates.push_back("C:\\Windows\\Fonts\\segoeui.ttf");

    for (const auto& path : candidates) {
        TTF_Font* f = TTF_OpenFont(path.c_str(), 96);
        if (f) return f;
    }
    return nullptr;
}

int main(int argc, char** argv) {
    // Si games.registry no está en el directorio actual (ej. se ejecutó desde build/bin), ajustar working dir
    FILE* testF = std::fopen(kRegistryPath, "r");
    if (!testF) {
        if (FILE* f2 = std::fopen("../../games.registry", "r")) {
            std::fclose(f2);
            if (chdir("../..") != 0) {}
        } else if (FILE* f1 = std::fopen("../games.registry", "r")) {
            std::fclose(f1);
            if (chdir("..") != 0) {}
        }
    } else {
        std::fclose(testF);
    }
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
                                       SDL_WINDOWPOS_CENTERED, 960, 640, SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!win) {
        std::fprintf(stderr, "No se pudo crear la ventana: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); // Activa Anti-Aliasing (Linear Filtering)
    
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        std::fprintf(stderr, "No se pudo crear el renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    
    // Mantiene la resolucion interna full HD
    SDL_RenderSetLogicalSize(ren, 1920, 1080);

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