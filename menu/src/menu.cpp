#include "menu.h"

Menu::Menu(SDL_Renderer* renderer, TTF_Font* font, const std::vector<GameEntry>& games)
    : renderer_(renderer), font_(font), games_(games) {
    openController();
}

Menu::~Menu() {
    closeController();
}

void Menu::openController() {
    if (controller_) return;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            controller_ = SDL_GameControllerOpen(i);
            if (controller_) break;
        }
    }
}

void Menu::closeController() {
    if (controller_) {
        SDL_GameControllerClose(controller_);
        controller_ = nullptr;
    }
}

void Menu::renderText(const std::string& text, int x, int y, SDL_Color color, float scale) {
    if (!font_) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font_, text.c_str(), color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
    SDL_FreeSurface(surf);
    if (!tex) return;

    int w = 0, h = 0;
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    SDL_Rect dst = {x, y, static_cast<int>(w * scale), static_cast<int>(h * scale)};
    SDL_RenderCopy(renderer_, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

void Menu::render() {
    SDL_SetRenderDrawColor(renderer_, 18, 18, 28, 255);
    SDL_RenderClear(renderer_);

    // Titulo
    renderText("MINI JUEGOS", 40, 28, SDL_Color{255, 255, 255, 255}, 2.2f);
    SDL_SetRenderDrawColor(renderer_, 70, 90, 140, 255);
    SDL_RenderFillRect(renderer_, &SDL_Rect{40, 92, 880, 3});

    // Lista de juegos
    int y = 130;
    for (size_t i = 0; i < games_.size(); ++i) {
        const bool sel = (i == static_cast<size_t>(selected_));
        if (sel) {
            SDL_SetRenderDrawColor(renderer_, 55, 80, 150, 255);
            SDL_RenderFillRect(renderer_, &SDL_Rect{40, y - 10, 880, 58});
        }
        renderText(games_[i].name, 60, y,
                   sel ? SDL_Color{255, 255, 255, 255} : SDL_Color{190, 190, 200, 255}, 1.6f);
        if (!games_[i].description.empty()) {
            renderText(games_[i].description, 60, y + 34, SDL_Color{150, 150, 165, 255}, 1.0f);
        }
        y += 76;
    }

    if (games_.empty()) {
        renderText("No hay juegos registrados. Edita games.registry", 60, 150,
                   SDL_Color{220, 120, 120, 255}, 1.2f);
    }

    // Mensaje de estado (p. ej. ejecutable no encontrado)
    if (!statusMessage_.empty() && SDL_GetTicks() < statusUntil_) {
        renderText(statusMessage_, 40, 500, SDL_Color{255, 180, 80, 255}, 1.1f);
    }

    // Instrucciones
    renderText("Arriba/Abajo: navegar   |   X (A): jugar   |   Circulo (B) / ESC: salir",
               40, 580, SDL_Color{150, 150, 165, 255}, 1.0f);

    SDL_RenderPresent(renderer_);
}

void Menu::moveUp() {
    if (!games_.empty()) {
        selected_ = (selected_ - 1 + static_cast<int>(games_.size())) % static_cast<int>(games_.size());
    }
}

void Menu::moveDown() {
    if (!games_.empty()) {
        selected_ = (selected_ + 1) % static_cast<int>(games_.size());
    }
}

void Menu::launchSelected() {
    if (games_.empty()) return;
    const GameEntry& g = games_[static_cast<size_t>(selected_)];
    if (g.executable.empty()) return;

    // Libera el mando para que el juego pueda abrirlo.
    closeController();

    int code = launcher_.launch(g.executable);
    if (code < 0) {
        statusMessage_ = "No se pudo lanzar: " + g.executable;
        statusUntil_ = SDL_GetTicks() + 4000;
    }

    // Reabre el mando al volver al menu.
    openController();
}

void Menu::handleEvent(const SDL_Event& ev) {
    switch (ev.type) {
        case SDL_QUIT:
            running_ = false;
            break;

        case SDL_KEYDOWN:
            switch (ev.key.keysym.sym) {
                case SDLK_UP:    moveUp(); break;
                case SDLK_DOWN:  moveDown(); break;
                case SDLK_RETURN:
                case SDLK_SPACE: launchSelected(); break;
                case SDLK_ESCAPE: running_ = false; break;
                default: break;
            }
            break;

        case SDL_CONTROLLERBUTTONDOWN:
            switch (ev.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_UP:   moveUp(); break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN: moveDown(); break;
                case SDL_CONTROLLER_BUTTON_A:         launchSelected(); break; // X en PS4
                case SDL_CONTROLLER_BUTTON_START:     launchSelected(); break;
                case SDL_CONTROLLER_BUTTON_B:         running_ = false; break; // Circulo en PS4
                default: break;
            }
            break;

        case SDL_CONTROLLERAXISMOTION:
            if (ev.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                const int v = ev.caxis.value;
                const bool wasNeutral = (lastAxisY_ >= -8000 && lastAxisY_ <= 8000);
                if (wasNeutral) {
                    if (v < -8000) moveUp();
                    else if (v > 8000) moveDown();
                }
                lastAxisY_ = v;
            }
            break;

        default:
            break;
    }
}

int Menu::run() {
    while (running_) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            handleEvent(ev);
        }
        render();
        SDL_Delay(16);
    }
    return 0;
}