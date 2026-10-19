#include "menu.h"
#include <cmath>

Menu::Menu(SDL_Renderer* renderer, TTF_Font* font, const std::vector<GameEntry>& games)
    : renderer_(renderer), font_(font), games_(games) {
    openController();
    hoverValues_.resize(games.size(), 0.0f);
}

Menu::~Menu() {
    closeController();
    for (auto& pair : textCache_) {
        SDL_DestroyTexture(pair.second);
    }
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

void Menu::renderText(const std::string& text, int x, int y, SDL_Color color, float scale, bool center) {
    if (!font_ || text.empty()) return;
    
    std::string key = text + "_" + std::to_string(color.r) + "_" + std::to_string(color.g) + "_" + std::to_string(color.b);
    SDL_Texture* tex = nullptr;
    
    auto it = textCache_.find(key);
    if (it == textCache_.end()) {
        SDL_Surface* surf = TTF_RenderUTF8_Blended(font_, text.c_str(), color);
        if (surf) {
            tex = SDL_CreateTextureFromSurface(renderer_, surf);
            SDL_FreeSurface(surf);
            textCache_[key] = tex;
        }
    } else {
        tex = it->second;
    }
    
    if (!tex) return;

    int w = 0, h = 0;
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    
    int dstW = static_cast<int>(w * scale);
    int dstH = static_cast<int>(h * scale);
    int dstX = center ? (x - dstW / 2) : x;
    int dstY = center ? (y - dstH / 2) : y;
    
    SDL_Rect dst = {dstX, dstY, dstW, dstH};
    SDL_RenderCopy(renderer_, tex, nullptr, &dst);
}

void Menu::render() {
    // Fondo claro con patron de cuadricula (Grid sutil)
    SDL_SetRenderDrawColor(renderer_, 250, 250, 250, 255);
    SDL_RenderClear(renderer_);

    float t_global = SDL_GetTicks() * 0.001f;

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 15); // Gris muy transparente
    int gridOffset = static_cast<int>(t_global * 40.0f) % 60; // El grid se mueve 40px por segundo
    for (int x = gridOffset - 60; x < 1920; x += 60) {
        SDL_RenderDrawLine(renderer_, x, 0, x, 1080);
    }
    for (int y = gridOffset - 60; y < 1080; y += 60) {
        SDL_RenderDrawLine(renderer_, 0, y, 1920, y);
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);

    if (awaitingStart_) {
        // Particulas procedurales animadas (Confeti brutalista flotando)
        SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 255);
        for (int i = 0; i < 20; i++) {
            float speed = 30.0f + (i % 5) * 15.0f; // Diferentes velocidades
            float yPos = 1200.0f - fmod((t_global * speed + i * 150.0f), 1400.0f);
            float xPos = fmod((i * 373.0f), 1920.0f);
            float wobble = sin(t_global * 1.5f + i) * 40.0f;
            
            int size = 20 + (i % 3) * 15;
            SDL_Rect r = { static_cast<int>(xPos + wobble), static_cast<int>(yPos), size, size };
            
            if (i % 2 == 0) {
                SDL_RenderFillRect(renderer_, &r); // Solido
            } else {
                for(int b=0; b<4; b++) { // Hueco
                    SDL_Rect br = {r.x+b, r.y+b, r.w-b*2, r.h-b*2};
                    SDL_RenderDrawRect(renderer_, &br);
                }
            }
        }

        // Titulo principal con caja brutalista gigante
        int titleW = 1200;
        int titleH = 200;
        int titleX = 1920 / 2 - titleW / 2;
        int titleY = 220;
        
        int shadowOffsetX = 16 + static_cast<int>(cos(t_global * 3.0f) * 6.0f);
        int shadowOffsetY = 16 + static_cast<int>(sin(t_global * 3.0f) * 6.0f);
        
        SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 255);
        SDL_Rect titleShadow = {titleX + shadowOffsetX, titleY + shadowOffsetY, titleW, titleH};
        SDL_RenderFillRect(renderer_, &titleShadow);
        
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        SDL_Rect titleBox = {titleX, titleY, titleW, titleH};
        SDL_RenderFillRect(renderer_, &titleBox);
        
        SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 255);
        for (int b = 0; b < 8; ++b) {
            SDL_Rect bR = {titleX + b, titleY + b, titleW - b*2, titleH - b*2};
            SDL_RenderDrawRect(renderer_, &bR);
        }
        
        renderText("DUAL SHOW", 1920 / 2, titleY + 100, SDL_Color{10, 10, 10, 255}, 1.8f, true);
        renderText("EXPERIENCIA SENSORIAL INTERACTIVA", 1920 / 2, 480, SDL_Color{80, 80, 80, 255}, 0.45f, true);

        // Boton "Start" palpitante (escala dinamica)
        float pulse = (sin(t_global * 8.0f) + 1.0f) * 0.5f; 
        float btnScale = 1.0f + 0.05f * pulse; // Crece 5%
        
        int baseW = 1000;
        int baseH = 130;
        int btnW = static_cast<int>(baseW * btnScale);
        int btnH = static_cast<int>(baseH * btnScale);
        int btnX = 1920 / 2 - btnW / 2;
        int btnY = 680 + static_cast<int>(sin(t_global * 2.5f) * 15.0f); 

        SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 255);
        SDL_Rect btnShadowRect = { btnX + 12, btnY + 12, btnW, btnH };
        SDL_RenderFillRect(renderer_, &btnShadowRect);

        int bgCol = 250 - static_cast<int>(30.0f * pulse);
        SDL_SetRenderDrawColor(renderer_, bgCol, bgCol, bgCol, 255);
        SDL_Rect btnRect = { btnX, btnY, btnW, btnH };
        SDL_RenderFillRect(renderer_, &btnRect);

        SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 255);
        for (int b = 0; b < 6; ++b) {
            SDL_Rect bRect = { btnX + b, btnY + b, btnW - b*2, btnH - b*2 };
            SDL_RenderDrawRect(renderer_, &bRect);
        }

        float textScale = 0.65f * btnScale;
        renderText("PULSA X PARA INICIAR", 1920 / 2, btnY + btnH/2, SDL_Color{10, 10, 10, 255}, textScale, true);

        SDL_RenderPresent(renderer_);
        return;
    }

    // Titulo masivo y moderno
    renderText("DUAL SHOW", 120, 80, SDL_Color{10, 10, 10, 255}, 1.0f);
    renderText("SELECCIONA UN PROYECTO", 130, 220, SDL_Color{100, 100, 100, 255}, 0.4f);

    // Línea gruesa debajo del título
    SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 255);
    SDL_Rect titleLine = {120, 280, 1600, 8};
    SDL_RenderFillRect(renderer_, &titleLine);

    // Activamos un "Clip Rect" para que las tarjetas no se dibujen sobre el titulo ni sobre el footer
    SDL_Rect clipRect = {0, 288, 1920, 712};
    SDL_RenderSetClipRect(renderer_, &clipRect);

    // Calculo de Scroll suave (Para cuando hay muchos juegos)
    float targetScrollY = 0;
    if (selected_ >= 2) {
        targetScrollY = -(selected_ - 1) * 240.0f; // Sube la camara 240px por cada item despues del segundo
    }
    scrollY_ += (targetScrollY - scrollY_) * 0.15f;

    // Dibujar tarjetas apiladas animadas
    int baseY = 340;
    for (size_t i = 0; i < games_.size(); ++i) {
        bool sel = (i == static_cast<size_t>(selected_));
        
        // Animacion suave (Lerp) para el efecto hover
        float target = sel ? 1.0f : 0.0f;
        hoverValues_[i] += (target - hoverValues_[i]) * 0.25f;
        float hv = hoverValues_[i];

        int offsetX = (i % 2 == 0) ? 0 : 60;
        int hoverOffset = static_cast<int>(-28.0f * hv); // Se levanta suavemente hacia arriba
        
        int x = 240 + offsetX;
        int y = baseY + hoverOffset + static_cast<int>(scrollY_);
        int w = 1100;
        int h = 210;

        // No dibujar si está muy arriba o muy abajo (Culling basico)
        if (y + h < 280 || y > 1000) {
            baseY += 240;
            continue;
        }

        // Sombra dura sólida negra con animacion (se extiende al levantarse)
        int shadowOffset = 16 + static_cast<int>(20.0f * hv);
        SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 255);
        SDL_Rect shadow = {x + shadowOffset, y + shadowOffset, w, h};
        SDL_RenderFillRect(renderer_, &shadow);

        // Fondo de la tarjeta (Transición de gris sutil a blanco puro)
        int cVal = 230 + static_cast<int>(25.0f * hv);
        SDL_SetRenderDrawColor(renderer_, cVal, cVal, cVal, 255);
        SDL_Rect card = {x, y, w, h};
        SDL_RenderFillRect(renderer_, &card);

        // Linea divisoria superior (estilo ventana web retro)
        SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 255);
        SDL_Rect headerLine = {x, y + 50, w, 6};
        SDL_RenderFillRect(renderer_, &headerLine);
        
        // "Botones" decorativos de la ventana en el encabezado
        SDL_Rect btn1 = {x + 30, y + 16, 20, 20};
        SDL_Rect btn2 = {x + 65, y + 16, 20, 20};
        SDL_RenderFillRect(renderer_, &btn1);
        SDL_RenderFillRect(renderer_, &btn2);

        // Borde grueso negro perimetral de la tarjeta
        for (int b = 0; b < 6; ++b) {
            SDL_Rect border = {x + b, y + b, w - b*2, h - b*2};
            SDL_RenderDrawRect(renderer_, &border);
        }

        // Textos del juego
        renderText(games_[i].name, x + 60, y + 75, SDL_Color{10, 10, 10, 255}, 0.7f);
        if (!games_[i].description.empty()) {
            renderText(games_[i].description, x + 60, y + 145, SDL_Color{80, 80, 80, 255}, 0.4f);
        }
        
        // Flecha interactiva a la izquierda (Solo se dibuja en la tarjeta seleccionada)
        if (sel) {
            int arrowX = static_cast<int>(x - 140 + (30.0f * hv));
            renderText("->", arrowX, y + 80, SDL_Color{10, 10, 10, 255}, 0.8f);
        }

        baseY += 240;
    }

    // Desactivar el Clip Rect para dibujar el resto de la interfaz normalmente
    SDL_RenderSetClipRect(renderer_, nullptr);

    if (games_.empty()) {
        renderText("No hay juegos registrados.", 120, 440, SDL_Color{10, 10, 10, 255}, 0.5f);
    }

    // Mensaje de estado
    if (!statusMessage_.empty() && SDL_GetTicks() < statusUntil_) {
        renderText(statusMessage_, 240, 950, SDL_Color{10, 10, 10, 255}, 0.5f);
    }

    // Footer (Barra inferior con controles)
    SDL_SetRenderDrawColor(renderer_, 250, 250, 250, 255);
    SDL_Rect footerBg = {0, 1000, 1920, 80};
    SDL_RenderFillRect(renderer_, &footerBg);

    // Footer (Barra inferior con controles)
    SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 255);
    SDL_Rect footerLine = {0, 1000, 1920, 8};
    SDL_RenderFillRect(renderer_, &footerLine);
    renderText("Arriba/Abajo: Navegar  |  X (A): Confirmar  |  Circulo (B) / ESC: Salir",
               40, 1024, SDL_Color{30, 30, 30, 255}, 0.4f);

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
    if (awaitingStart_) {
        if (ev.type == SDL_KEYDOWN || ev.type == SDL_CONTROLLERBUTTONDOWN) {
            awaitingStart_ = false;
        }
        if (ev.type == SDL_QUIT) {
            running_ = false;
        }
        return;
    }

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