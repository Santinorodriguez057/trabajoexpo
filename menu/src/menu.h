#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

#include <string>
#include <vector>

#include "game_launcher.h"
#include "game_registry.h"

// Menu principal: lista los juegos del registro y los lanza.
class Menu {
public:
    Menu(SDL_Renderer* renderer, TTF_Font* font, const std::vector<GameEntry>& games);
    ~Menu();

    // Bucle principal. Devuelve el codigo de salida del menu.
    int run();

private:
    void openController();
    void closeController();
    void render();
    void renderText(const std::string& text, int x, int y, SDL_Color color, float scale);
    void handleEvent(const SDL_Event& ev);
    void moveUp();
    void moveDown();
    void launchSelected();

    SDL_Renderer* renderer_;
    TTF_Font* font_;
    const std::vector<GameEntry>& games_;
    SDL_GameController* controller_ = nullptr;

    int selected_ = 0;
    bool running_ = true;
    int lastAxisY_ = 0;

    std::string statusMessage_;
    Uint32 statusUntil_ = 0;

    GameLauncher launcher_;
};