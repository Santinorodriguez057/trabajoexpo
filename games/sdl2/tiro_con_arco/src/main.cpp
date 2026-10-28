#include <SDL.h>
#include <SDL_ttf.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "config.h"
#include "archery_game.h"

static TTF_Font* loadSystemFont(int ptSize) {
    std::vector<std::string> candidates = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    };

    const char* winDir = std::getenv("WINDIR");
    if (!winDir) winDir = std::getenv("SystemRoot");
    std::string winBase = winDir ? winDir : "C:\\Windows";
    candidates.push_back(winBase + "\\Fonts\\arialbd.ttf");
    candidates.push_back(winBase + "\\Fonts\\arial.ttf");
    candidates.push_back(winBase + "\\Fonts\\segoeuib.ttf");
    candidates.push_back(winBase + "\\Fonts\\segoeui.ttf");
    candidates.push_back(winBase + "\\Fonts\\tahoma.ttf");
    candidates.push_back("C:\\Windows\\Fonts\\arial.ttf");

    for (const auto& path : candidates) {
        TTF_Font* f = TTF_OpenFont(path.c_str(), ptSize);
        if (f) return f;
    }
    return nullptr;
}

int main(int argc, char** argv) {
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_SENSOR | SDL_INIT_AUDIO) < 0) {
        std::fprintf(stderr, "Error SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() < 0) {
        std::fprintf(stderr, "Error TTF_Init: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Tiro al Blanco - Puesto de Feria",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::fprintf(stderr, "Error al crear ventana: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    SDL_Renderer* ren = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!ren) {
        std::fprintf(stderr, "Error al crear renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Resolución lógica Full HD 1920x1080
    SDL_RenderSetLogicalSize(ren, 1920, 1080);

    TTF_Font* fontLarge = loadSystemFont(36);
    TTF_Font* fontMed = loadSystemFont(26);
    TTF_Font* fontSmall = loadSystemFont(18);

    GameConfig config;
    config.load();

    ArcheryGame game(ren, fontLarge, fontMed, fontSmall, config);
    int exitCode = game.run();

    if (fontLarge) TTF_CloseFont(fontLarge);
    if (fontMed) TTF_CloseFont(fontMed);
    if (fontSmall) TTF_CloseFont(fontSmall);

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return exitCode;
}
