#include "archery_game.h"
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstdlib>

ArcheryGame::ArcheryGame(SDL_Renderer* ren, TTF_Font* fontLarge, TTF_Font* fontMed, TTF_Font* fontSmall, GameConfig& config)
    : ren_(ren), fontLarge_(fontLarge), fontMed_(fontMed), fontSmall_(fontSmall), config_(config), input_(config) {
}

ArcheryGame::~ArcheryGame() {
    config_.save();
}

int ArcheryGame::run() {
    input_.init();
    audio_.init();
    config_.load();

    startRound(1);
    state_ = GameState::CALIBRATION;

    Uint32 lastTime = SDL_GetTicks();

    while (running_) {
        Uint32 currentTime = SDL_GetTicks();
        float dt = (currentTime - lastTime) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f;
        lastTime = currentTime;

        handleEvents();
        update(dt);
        render();

        SDL_Delay(8);
    }

    return 0;
}

void ArcheryGame::startRound(int roundNumber) {
    currentRound_ = roundNumber;
    arrowsRemaining_ = kArrowsPerRound;
    roundScore_ = 0;
    stuckArrows_.clear();
    activeArrow_.active = false;

    // Distancia progresiva con ligera aleatoriedad
    // Rondas 1-3: 18m - 28m
    // Rondas 4-7: 32m - 48m
    // Rondas 8-10: 52m - 70m
    float baseDist = 18.0f + (roundNumber - 1) * 5.5f;
    float distVariation = ((rand() % 100) / 100.0f - 0.5f) * 4.0f;
    targetDistance_ = std::clamp(baseDist + distVariation, 15.0f, 75.0f);

    // Variación horizontal y de altura en rondas intermedias y avanzadas
    if (roundNumber > 2) {
        targetX_ = ((rand() % 100) / 100.0f - 0.5f) * 2.2f;
        targetY_ = 1.3f + ((rand() % 100) / 100.0f - 0.5f) * 0.6f;
    } else {
        targetX_ = 0.0f;
        targetY_ = 1.3f;
    }

    targetRadius_ = 0.85f; // Radio visual cómodo para tiro con arco
}

void ArcheryGame::handleEvents() {
    input_.clearActionFlags();

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            running_ = false;
            return;
        }

        input_.handleEvent(ev);

        // Controles de navegación en menús
        if (state_ == GameState::PAUSE_MENU) {
            if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_UP) pauseMenuIndex_ = (pauseMenuIndex_ + 6) % 7;
                else if (ev.key.keysym.sym == SDLK_DOWN) pauseMenuIndex_ = (pauseMenuIndex_ + 1) % 7;
                else if (ev.key.keysym.sym == SDLK_LEFT) {
                    if (pauseMenuIndex_ == 0) config_.sensitivityX = std::max(0.2f, config_.sensitivityX - 0.1f);
                    else if (pauseMenuIndex_ == 1) config_.sensitivityY = std::max(0.2f, config_.sensitivityY - 0.1f);
                    else if (pauseMenuIndex_ == 2) config_.smoothing = std::max(0.0f, config_.smoothing - 0.05f);
                    else if (pauseMenuIndex_ == 3) config_.deadzone = std::max(0.0f, config_.deadzone - 0.01f);
                    else if (pauseMenuIndex_ == 4) config_.invertY = !config_.invertY;
                    else if (pauseMenuIndex_ == 5) config_.rumbleEnabled = !config_.rumbleEnabled;
                } else if (ev.key.keysym.sym == SDLK_RIGHT) {
                    if (pauseMenuIndex_ == 0) config_.sensitivityX = std::min(3.0f, config_.sensitivityX + 0.1f);
                    else if (pauseMenuIndex_ == 1) config_.sensitivityY = std::min(3.0f, config_.sensitivityY + 0.1f);
                    else if (pauseMenuIndex_ == 2) config_.smoothing = std::min(0.9f, config_.smoothing + 0.05f);
                    else if (pauseMenuIndex_ == 3) config_.deadzone = std::min(0.2f, config_.deadzone + 0.01f);
                    else if (pauseMenuIndex_ == 4) config_.invertY = !config_.invertY;
                    else if (pauseMenuIndex_ == 5) config_.rumbleEnabled = !config_.rumbleEnabled;
                } else if (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_SPACE) {
                    if (pauseMenuIndex_ == 6) {
                        // Reanudar
                        state_ = GameState::PLAYING;
                        config_.save();
                    }
                }
            } else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
                if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) pauseMenuIndex_ = (pauseMenuIndex_ + 6) % 7;
                else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) pauseMenuIndex_ = (pauseMenuIndex_ + 1) % 7;
                else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
                    if (pauseMenuIndex_ == 0) config_.sensitivityX = std::max(0.2f, config_.sensitivityX - 0.1f);
                    else if (pauseMenuIndex_ == 1) config_.sensitivityY = std::max(0.2f, config_.sensitivityY - 0.1f);
                    else if (pauseMenuIndex_ == 2) config_.smoothing = std::max(0.0f, config_.smoothing - 0.05f);
                    else if (pauseMenuIndex_ == 3) config_.deadzone = std::max(0.0f, config_.deadzone - 0.01f);
                    else if (pauseMenuIndex_ == 4) config_.invertY = !config_.invertY;
                    else if (pauseMenuIndex_ == 5) config_.rumbleEnabled = !config_.rumbleEnabled;
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
                    if (pauseMenuIndex_ == 0) config_.sensitivityX = std::min(3.0f, config_.sensitivityX + 0.1f);
                    else if (pauseMenuIndex_ == 1) config_.sensitivityY = std::min(3.0f, config_.sensitivityY + 0.1f);
                    else if (pauseMenuIndex_ == 2) config_.smoothing = std::min(0.9f, config_.smoothing + 0.05f);
                    else if (pauseMenuIndex_ == 3) config_.deadzone = std::min(0.2f, config_.deadzone + 0.01f);
                    else if (pauseMenuIndex_ == 4) config_.invertY = !config_.invertY;
                    else if (pauseMenuIndex_ == 5) config_.rumbleEnabled = !config_.rumbleEnabled;
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                    if (pauseMenuIndex_ == 6) {
                        state_ = GameState::PLAYING;
                        config_.save();
                    }
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
                    state_ = GameState::PLAYING;
                    config_.save();
                }
            }
        }
    }
}

void ArcheryGame::update(float dt) {
    input_.update(dt);
    const AimState& aim = input_.getState();

    // Gestión del sonido y animación de tensar
    if (state_ == GameState::PLAYING && aim.isDrawing) {
        audio_.playDraw(aim.tension);
    }

    // Vibración de la cuerda del arco al disparar
    if (stringVibeTimer_ > 0.0f) {
        stringVibeTimer_ -= dt;
        stringVibration_ = std::sin(stringVibeTimer_ * 55.0f) * (stringVibeTimer_ / 0.35f);
    } else {
        stringVibration_ = 0.0f;
    }

    // Wobble de la diana por impacto
    if (targetWobble_ > 0.0f) {
        targetWobble_ = std::max(0.0f, targetWobble_ - dt * 3.5f);
    }

    // Actualizar textos flotantes
    for (auto it = floatingTexts_.begin(); it != floatingTexts_.end();) {
        it->life -= dt;
        it->y -= dt * 45.0f; // Sube suavemente
        if (it->life <= 0.0f) {
            it = floatingTexts_.erase(it);
        } else {
            ++it;
        }
    }

    switch (state_) {
        case GameState::CALIBRATION:
            if (input_.confirmPressed()) {
                input_.calibrate();
                state_ = GameState::PLAYING;
            }
            break;

        case GameState::PLAYING:
            if (input_.pausePressed()) {
                state_ = GameState::PAUSE_MENU;
                pauseMenuIndex_ = 0;
                break;
            }

            // Disparar flecha cuando R2 se suelta
            if (aim.justFired && !activeArrow_.active && arrowsRemaining_ > 0) {
                shootArrow(aim.releaseTension);
            }

            // Simulación física de la flecha en vuelo
            if (activeArrow_.active) {
                // Guardar estela
                SDL_Point pScreen = project3D(activeArrow_.x, activeArrow_.y, activeArrow_.z);
                activeArrow_.trail.push_back({static_cast<float>(pScreen.x), static_cast<float>(pScreen.y)});
                if (activeArrow_.trail.size() > 18) activeArrow_.trail.erase(activeArrow_.trail.begin());

                // Integración balística con gravedad
                activeArrow_.vy -= 9.8f * dt;
                activeArrow_.x += activeArrow_.vx * dt;
                activeArrow_.y += activeArrow_.vy * dt;
                activeArrow_.z += activeArrow_.vz * dt;

                // Impacto en el plano de la diana
                if (activeArrow_.z >= targetDistance_) {
                    evaluateImpact(activeArrow_.x, activeArrow_.y);
                    activeArrow_.active = false;
                    activeArrow_.trail.clear();
                    arrowsRemaining_--;

                    if (arrowsRemaining_ <= 0) {
                        // Fin de la ronda
                        if (currentRound_ < kTotalRounds) {
                            state_ = GameState::ROUND_SUMMARY;
                            audio_.playFanfare();
                        } else {
                            state_ = GameState::MATCH_OVER;
                            audio_.playFanfare();
                            if (totalScore_ > config_.highRecord) {
                                isNewRecord_ = true;
                                config_.highRecord = totalScore_;
                                config_.save();
                            }
                        }
                    }
                }
            }
            break;

        case GameState::ROUND_SUMMARY:
            if (input_.confirmPressed()) {
                startRound(currentRound_ + 1);
                state_ = GameState::PLAYING;
            } else if (input_.cancelPressed()) {
                running_ = false; // Salir al menú
            }
            break;

        case GameState::MATCH_OVER:
            if (input_.confirmPressed()) {
                // Nueva partida
                totalScore_ = 0;
                totalHits_ = 0;
                totalBullseyes_ = 0;
                bestShotPoints_ = 0;
                isNewRecord_ = false;
                startRound(1);
                state_ = GameState::PLAYING;
            } else if (input_.cancelPressed()) {
                running_ = false;
            }
            break;

        case GameState::PAUSE_MENU:
            if (input_.cancelPressed() || input_.pausePressed()) {
                state_ = GameState::PLAYING;
                config_.save();
            }
            break;
    }
}

void ArcheryGame::shootArrow(float tension) {
    audio_.playShoot();

    // Animación de rebote de la cuerda
    stringVibeTimer_ = 0.35f;

    const AimState& aim = input_.getState();

    activeArrow_.active = true;
    activeArrow_.trail.clear();

    // Punto de origen del arco en coordenadas 3D (a la derecha y abajo de la cámara)
    activeArrow_.x = 0.20f;
    activeArrow_.y = 1.05f;
    activeArrow_.z = 0.6f;

    // Velocidad de salida según la tensión alcanzada (35 m/s a 80 m/s)
    float speed = 35.0f + tension * 45.0f;
    activeArrow_.initialSpeed = speed;

    // Dirección hacia el punto de mira proyectado en el plano de la diana
    // aim.x y aim.y representan el ángulo de orientación del DualShock
    float aimWorldX = targetX_ + aim.x * (targetDistance_ * 0.14f);
    float aimWorldY = targetY_ - aim.y * (targetDistance_ * 0.12f);

    float dx = aimWorldX - activeArrow_.x;
    float dy = aimWorldY - activeArrow_.y;
    float dz = targetDistance_ - activeArrow_.z;

    float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
    activeArrow_.vx = (dx / dist) * speed;
    activeArrow_.vy = (dy / dist) * speed;
    activeArrow_.vz = (dz / dist) * speed;
}

void ArcheryGame::evaluateImpact(float hitX, float hitY) {
    float diffX = hitX - targetX_;
    float diffY = hitY - targetY_;
    float dist = std::sqrt(diffX * diffX + diffY * diffY);

    SDL_Point screenHit = project3D(targetX_, targetY_, targetDistance_);

    if (dist <= targetRadius_) {
        // Impacto válido en la diana
        totalHits_++;
        targetWobble_ = 1.0f;

        // Calcular puntuación según anillos oficiales de arquería (10 zonas concéntricas)
        float rFrac = dist / targetRadius_;
        int pts = 0;
        std::string label;
        SDL_Color color;

        if (rFrac <= 0.10f) {
            // Diana perfecta (X-ring)
            pts = 100;
            label = "¡CENTRO PERFECTO! +100";
            color = {255, 220, 40, 255};
            totalBullseyes_++;
            audio_.playBullseye();
            input_.setRumble(1.0f, 1.0f, 250);
            input_.setLED(255, 230, 80);
        } else if (rFrac <= 0.20f) {
            // Anillo 9 (Oro)
            pts = 80;
            label = "¡DIANA DORADA! +80";
            color = {255, 205, 30, 255};
            audio_.playBullseye();
            input_.setRumble(0.7f, 0.9f, 200);
            input_.setLED(255, 200, 30);
        } else if (rFrac <= 0.40f) {
            // Anillos 8 y 7 (Rojo)
            pts = (rFrac <= 0.30f) ? 60 : 50;
            label = "ZONA ROJA +" + std::to_string(pts);
            color = {235, 60, 60, 255};
            audio_.playImpact();
            input_.setRumble(0.5f, 0.6f, 160);
        } else if (rFrac <= 0.60f) {
            // Anillos 6 y 5 (Azul)
            pts = (rFrac <= 0.50f) ? 40 : 30;
            label = "ZONA AZUL +" + std::to_string(pts);
            color = {40, 160, 240, 255};
            audio_.playImpact();
            input_.setRumble(0.4f, 0.5f, 140);
        } else if (rFrac <= 0.80f) {
            // Anillos 4 y 3 (Negro)
            pts = (rFrac <= 0.70f) ? 20 : 15;
            label = "ZONA NEGRA +" + std::to_string(pts);
            color = {180, 180, 190, 255};
            audio_.playImpact();
            input_.setRumble(0.3f, 0.4f, 120);
        } else {
            // Anillos 2 y 1 (Blanco)
            pts = (rFrac <= 0.90f) ? 10 : 5;
            label = "ZONA BLANCA +" + std::to_string(pts);
            color = {240, 240, 245, 255};
            audio_.playImpact();
            input_.setRumble(0.2f, 0.3f, 100);
        }

        totalScore_ += pts;
        roundScore_ += pts;
        if (pts > bestShotPoints_) bestShotPoints_ = pts;

        // Registrar flecha clavada en la diana
        float angle = ((rand() % 60) - 30) * 0.01f;
        stuckArrows_.push_back({diffX, diffY, angle});

        // Banner flotante animado de impacto
        floatingTexts_.push_back({label, static_cast<float>(screenHit.x), static_cast<float>(screenHit.y - 70), 2.2f, 2.2f, color, 1.2f});

    } else {
        // Fallo fuera de la diana
        audio_.playImpact();
        input_.setRumble(0.2f, 0.2f, 80);
        floatingTexts_.push_back({"FALLO 0", static_cast<float>(screenHit.x), static_cast<float>(screenHit.y - 40), 1.8f, 1.8f, {200, 70, 70, 255}, 1.0f});
    }
}

SDL_Point ArcheryGame::project3D(float x, float y, float z) {
    if (z <= 0.1f) z = 0.1f;
    const float fov = 2200.0f;
    int px = 960 + static_cast<int>((x / z) * fov);
    int py = 540 - static_cast<int>(((y - 1.3f) / z) * fov);
    return {px, py};
}

void ArcheryGame::drawText(TTF_Font* font, const std::string& text, int x, int y, SDL_Color color, bool center) {
    if (!font || text.empty()) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren_, surf);
    SDL_FreeSurface(surf);
    if (!tex) return;

    int w, h;
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    SDL_Rect dst = {center ? x - w / 2 : x, y, w, h};
    SDL_RenderCopy(ren_, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

void ArcheryGame::drawCircleFilled(int cx, int cy, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(ren_, color.r, color.g, color.b, color.a);
    for (int w = 0; w <= radius * 2; w++) {
        int dx = radius - w;
        int dy = static_cast<int>(std::sqrt(radius * radius - dx * dx));
        SDL_RenderDrawLine(ren_, cx + dx, cy - dy, cx + dx, cy + dy);
    }
}

void ArcheryGame::drawCircleOutline(int cx, int cy, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(ren_, color.r, color.g, color.b, color.a);
    int x = radius - 1;
    int y = 0;
    int dx = 1;
    int dy = 1;
    int err = dx - (radius << 1);

    while (x >= y) {
        SDL_RenderDrawPoint(ren_, cx + x, cy + y);
        SDL_RenderDrawPoint(ren_, cx + y, cy + x);
        SDL_RenderDrawPoint(ren_, cx - y, cy + x);
        SDL_RenderDrawPoint(ren_, cx - x, cy + y);
        SDL_RenderDrawPoint(ren_, cx - x, cy - y);
        SDL_RenderDrawPoint(ren_, cx - y, cy - x);
        SDL_RenderDrawPoint(ren_, cx + y, cy - x);
        SDL_RenderDrawPoint(ren_, cx + x, cy - y);

        if (err <= 0) {
            y++;
            err += dy;
            dy += 2;
        }
        if (err > 0) {
            x--;
            dx += 2;
            err += dx - (radius << 1);
        }
    }
}

void ArcheryGame::render() {
    SDL_SetRenderDrawColor(ren_, 15, 20, 25, 255);
    SDL_RenderClear(ren_);

    renderBackground();
    renderTarget();
    renderFlyingArrow();
    renderFirstPersonBow();
    renderHUD();

    // Renderizar textos flotantes
    for (const auto& ft : floatingTexts_) {
        float alpha = std::clamp(ft.life / (ft.maxLife * 0.4f), 0.0f, 1.0f);
        SDL_Color c = ft.color;
        c.a = static_cast<Uint8>(alpha * 255.0f);
        drawText(fontMed_, ft.text, static_cast<int>(ft.x), static_cast<int>(ft.y), c, true);
    }

    if (state_ == GameState::CALIBRATION) renderCalibrationScreen();
    else if (state_ == GameState::ROUND_SUMMARY) renderRoundSummary();
    else if (state_ == GameState::MATCH_OVER) renderMatchOver();
    else if (state_ == GameState::PAUSE_MENU) renderPauseMenu();

    SDL_RenderPresent(ren_);
}

void ArcheryGame::renderBackground() {
    // 1. Cielo con degradado sereno
    for (int y = 0; y < 540; ++y) {
        float t = y / 540.0f;
        Uint8 r = static_cast<Uint8>(24 + t * 60);
        Uint8 g = static_cast<Uint8>(65 + t * 90);
        Uint8 b = static_cast<Uint8>(120 + t * 65);
        SDL_SetRenderDrawColor(ren_, r, g, b, 255);
        SDL_RenderDrawLine(ren_, 0, y, 1920, y);
    }

    // 2. Siluetas de montañas lejanas
    SDL_SetRenderDrawColor(ren_, 45, 80, 95, 255);
    for (int x = 0; x < 1920; x += 3) {
        int mountainY = 480 - static_cast<int>(std::sin(x * 0.003f) * 70.0f + std::cos(x * 0.009f) * 40.0f);
        SDL_RenderDrawLine(ren_, x, mountainY, x, 540);
    }

    // 3. Suelo de césped con perspectiva
    for (int y = 540; y < 1080; ++y) {
        float t = (y - 540) / 540.0f;
        Uint8 r = static_cast<Uint8>(35 + t * 30);
        Uint8 g = static_cast<Uint8>(100 + t * 65);
        Uint8 b = static_cast<Uint8>(40 + t * 25);
        SDL_SetRenderDrawColor(ren_, r, g, b, 255);
        SDL_RenderDrawLine(ren_, 0, y, 1920, y);
    }

    // Líneas de carril de tiro hacia el punto de fuga
    SDL_SetRenderDrawColor(ren_, 80, 160, 90, 80);
    SDL_RenderDrawLine(ren_, 960, 540, 200, 1080);
    SDL_RenderDrawLine(ren_, 960, 540, 1720, 1080);
    SDL_RenderDrawLine(ren_, 960, 540, 600, 1080);
    SDL_RenderDrawLine(ren_, 960, 540, 1320, 1080);

    // Banderines de distancia (20m, 30m, 50m, 70m)
    std::vector<float> distMarkers = {20.0f, 30.0f, 50.0f, 70.0f};
    for (float d : distMarkers) {
        SDL_Point mP = project3D(-2.8f, 1.3f, d);
        if (mP.y >= 540 && mP.y <= 1060) {
            SDL_SetRenderDrawColor(ren_, 240, 240, 240, 200);
            SDL_RenderDrawLine(ren_, mP.x, mP.y, mP.x, mP.y - 25);
            std::string markerTxt = std::to_string(static_cast<int>(d)) + "m";
            drawText(fontSmall_, markerTxt, mP.x + 5, mP.y - 30, {220, 230, 240, 220});
        }
    }
}

void ArcheryGame::renderTarget() {
    float wobbleOffset = std::sin(SDL_GetTicks() * 0.04f) * targetWobble_ * 0.06f;
    SDL_Point center = project3D(targetX_ + wobbleOffset, targetY_, targetDistance_);

    const float fov = 2200.0f;
    int radiusPx = static_cast<int>((targetRadius_ / targetDistance_) * fov);
    if (radiusPx < 6) radiusPx = 6;

    // 1. Patas de madera del trípode (A-Frame)
    int legBottomY = center.y + static_cast<int>(radiusPx * 1.7f);
    SDL_SetRenderDrawColor(ren_, 115, 75, 45, 255);
    SDL_RenderDrawLine(ren_, center.x - static_cast<int>(radiusPx * 0.9f), legBottomY, center.x, center.y);
    SDL_RenderDrawLine(ren_, center.x + static_cast<int>(radiusPx * 0.9f), legBottomY, center.x, center.y);
    SDL_RenderDrawLine(ren_, center.x, legBottomY + 10, center.x, center.y);

    // 2. Soporte redondo de paja detrás de la diana
    drawCircleFilled(center.x, center.y, radiusPx + 6, {170, 140, 90, 255});
    drawCircleOutline(center.x, center.y, radiusPx + 6, {95, 75, 45, 255});

    // 3. Anillos concéntricos oficiales de la diana (FITA)
    // Blanco (1-2)
    drawCircleFilled(center.x, center.y, radiusPx, {245, 245, 250, 255});
    drawCircleOutline(center.x, center.y, static_cast<int>(radiusPx * 0.9f), {180, 180, 190, 255});

    // Negro (3-4)
    drawCircleFilled(center.x, center.y, static_cast<int>(radiusPx * 0.8f), {35, 35, 40, 255});
    drawCircleOutline(center.x, center.y, static_cast<int>(radiusPx * 0.7f), {80, 80, 90, 255});

    // Azul (5-6)
    drawCircleFilled(center.x, center.y, static_cast<int>(radiusPx * 0.6f), {30, 145, 235, 255});
    drawCircleOutline(center.x, center.y, static_cast<int>(radiusPx * 0.5f), {70, 180, 255, 255});

    // Rojo (7-8)
    drawCircleFilled(center.x, center.y, static_cast<int>(radiusPx * 0.4f), {230, 45, 50, 255});
    drawCircleOutline(center.x, center.y, static_cast<int>(radiusPx * 0.3f), {255, 90, 95, 255});

    // Oro (9-10)
    drawCircleFilled(center.x, center.y, static_cast<int>(radiusPx * 0.2f), {255, 210, 30, 255});
    // Centro X
    drawCircleFilled(center.x, center.y, static_cast<int>(radiusPx * 0.1f), {255, 235, 100, 255});
    drawCircleOutline(center.x, center.y, static_cast<int>(radiusPx * 0.1f), {200, 150, 10, 255});

    // 4. Flechas clavadas en la diana
    for (const auto& sa : stuckArrows_) {
        int arrowX = center.x + static_cast<int>((sa.localX / targetDistance_) * fov);
        int arrowY = center.y - static_cast<int>((sa.localY / targetDistance_) * fov);

        // Varilla de la flecha clavada proyectada hacia la cámara
        int shaftLen = std::max(12, static_cast<int>(radiusPx * 0.45f));
        int tailX = arrowX + static_cast<int>(std::sin(sa.angle) * shaftLen);
        int tailY = arrowY + static_cast<int>(std::cos(sa.angle) * shaftLen);

        SDL_SetRenderDrawColor(ren_, 190, 170, 130, 255);
        SDL_RenderDrawLine(ren_, arrowX, arrowY, tailX, tailY);

        // Plumas fletching rojas / blancas
        SDL_SetRenderDrawColor(ren_, 235, 50, 50, 255);
        SDL_RenderDrawLine(ren_, tailX - 3, tailY - 3, tailX + 3, tailY + 3);
        drawCircleFilled(tailX, tailY, 3, {255, 255, 255, 255});
    }
}

void ArcheryGame::renderFlyingArrow() {
    if (!activeArrow_.active) return;

    // Estela de la flecha
    if (activeArrow_.trail.size() > 1) {
        for (size_t i = 0; i < activeArrow_.trail.size() - 1; ++i) {
            float alpha = static_cast<float>(i) / activeArrow_.trail.size();
            Uint8 a = static_cast<Uint8>(alpha * 180.0f);
            SDL_SetRenderDrawColor(ren_, 255, 255, 255, a);
            SDL_RenderDrawLine(ren_, static_cast<int>(activeArrow_.trail[i].x), static_cast<int>(activeArrow_.trail[i].y),
                                     static_cast<int>(activeArrow_.trail[i+1].x), static_cast<int>(activeArrow_.trail[i+1].y));
        }
    }

    // Punta y cuerpo de la flecha
    SDL_Point tip = project3D(activeArrow_.x, activeArrow_.y, activeArrow_.z);
    SDL_Point nock = project3D(activeArrow_.x - activeArrow_.vx * 0.015f,
                               activeArrow_.y - activeArrow_.vy * 0.015f,
                               activeArrow_.z - activeArrow_.vz * 0.015f);

    SDL_SetRenderDrawColor(ren_, 210, 190, 140, 255);
    SDL_RenderDrawLine(ren_, tip.x, tip.y, nock.x, nock.y);
    drawCircleFilled(tip.x, tip.y, 3, {220, 220, 230, 255});
}

void ArcheryGame::renderFirstPersonBow() {
    const AimState& aim = input_.getState();

    // 1. Retícula de apuntado físico (Sigue la orientación del DualShock)
    int crossX = 960 + static_cast<int>(aim.x * 750.0f);
    int crossY = 540 + static_cast<int>(aim.y * 450.0f);

    int ringRadius = 24 - static_cast<int>(aim.tension * 8.0f); // Se cierra al tensar
    SDL_Color reticleColor = aim.isDrawing ? SDL_Color{255, 180, 40, 240} : SDL_Color{220, 240, 255, 200};

    drawCircleOutline(crossX, crossY, ringRadius, reticleColor);
    drawCircleOutline(crossX, crossY, 3, reticleColor);
    // Marcas de elevación y viento
    SDL_SetRenderDrawColor(ren_, reticleColor.r, reticleColor.g, reticleColor.b, 180);
    SDL_RenderDrawLine(ren_, crossX - ringRadius - 6, crossY, crossX - ringRadius + 2, crossY);
    SDL_RenderDrawLine(ren_, crossX + ringRadius - 2, crossY, crossX + ringRadius + 6, crossY);
    SDL_RenderDrawLine(ren_, crossX, crossY - ringRadius - 6, crossX, crossY - ringRadius + 2);
    SDL_RenderDrawLine(ren_, crossX, crossY + ringRadius - 2, crossX, crossY + ringRadius + 14);

    // 2. Arco en primera persona (Pala superior, pala inferior, cuerda y flecha en reposo/tensado)
    // El arco se inclina ligeramente con el movimiento
    int bowBaseX = 1450 + static_cast<int>(aim.x * 80.0f);
    int bowBaseY = 820 + static_cast<int>(aim.y * 60.0f);

    // Tensión retrae el nock hacia el ojo del jugador
    float drawBack = aim.tension * 280.0f;
    int nockX = bowBaseX - 160 - static_cast<int>(drawBack);
    int nockY = bowBaseY - static_cast<int>(stringVibration_ * 15.0f);

    // Puntas de las palas del arco
    int topTipX = bowBaseX - 40 - static_cast<int>(aim.tension * 40.0f);
    int topTipY = bowBaseY - 380;
    int botTipX = bowBaseX - 40 - static_cast<int>(aim.tension * 40.0f);
    int botTipY = bowBaseY + 280;

    // Cuerpo del arco (Riser y palas de madera/carbono)
    SDL_SetRenderDrawColor(ren_, 135, 75, 40, 255); // Madera noble
    for (int t = -4; t <= 4; ++t) {
        SDL_RenderDrawLine(ren_, bowBaseX + t, bowBaseY - 140, topTipX + t, topTipY);
        SDL_RenderDrawLine(ren_, bowBaseX + t, bowBaseY + 120, botTipX + t, botTipY);
    }
    // Empuñadura central
    drawCircleFilled(bowBaseX, bowBaseY, 20, {45, 45, 50, 255});

    // Cuerda del arco (Top -> Nock -> Bottom)
    SDL_SetRenderDrawColor(ren_, 245, 245, 255, 230);
    SDL_RenderDrawLine(ren_, topTipX, topTipY, nockX, nockY);
    SDL_RenderDrawLine(ren_, botTipX, botTipY, nockX, nockY);

    // Flecha apoyada en la cuerda (si quedan flechas y no está en vuelo)
    if (arrowsRemaining_ > 0 && !activeArrow_.active) {
        int arrowTipX = bowBaseX + 220;
        int arrowTipY = bowBaseY - 4;

        // Vástago de la flecha
        SDL_SetRenderDrawColor(ren_, 215, 185, 130, 255);
        SDL_RenderDrawLine(ren_, nockX, nockY, arrowTipX, arrowTipY);
        SDL_RenderDrawLine(ren_, nockX, nockY + 1, arrowTipX, arrowTipY + 1);

        // Punta de acero
        drawCircleFilled(arrowTipX, arrowTipY, 4, {220, 220, 230, 255});

        // Plumas (Fletchings) rojas en el nock
        SDL_SetRenderDrawColor(ren_, 235, 40, 40, 255);
        SDL_RenderDrawLine(ren_, nockX + 5, nockY - 8, nockX + 28, nockY - 1);
        SDL_RenderDrawLine(ren_, nockX + 5, nockY + 8, nockX + 28, nockY + 1);
    }
}

void ArcheryGame::renderHUD() {
    // 1. Barra superior de información
    SDL_Rect topBar = {0, 0, 1920, 85};
    SDL_SetRenderDrawColor(ren_, 15, 25, 35, 210);
    SDL_RenderFillRect(ren_, &topBar);
    SDL_SetRenderDrawColor(ren_, 45, 80, 110, 180);
    SDL_RenderDrawLine(ren_, 0, 85, 1920, 85);

    // Ronda actual
    std::string roundStr = "RONDA " + std::to_string(currentRound_) + " / " + std::to_string(kTotalRounds);
    drawText(fontMed_, roundStr, 60, 24, {240, 245, 250, 255});

    // Distancia del objetivo
    std::string distStr = "DISTANCIA: " + std::to_string(static_cast<int>(targetDistance_)) + " METROS";
    drawText(fontMed_, distStr, 960, 24, {255, 215, 60, 255}, true);

    // Puntos acumulados
    std::string scoreStr = "PUNTOS: " + std::to_string(totalScore_);
    drawText(fontLarge_, scoreStr, 1600, 16, {255, 255, 255, 255});

    // 2. Carcaj de flechas restantes (Abajo a la izquierda)
    int quiverX = 60;
    int quiverY = 1000;
    drawText(fontSmall_, "FLECHAS RESTANTES:", quiverX, quiverY - 32, {200, 215, 230, 220});
    for (int i = 0; i < kArrowsPerRound; ++i) {
        SDL_Color arrowColor = (i < arrowsRemaining_) ? SDL_Color{80, 220, 120, 255} : SDL_Color{70, 80, 90, 160};
        drawCircleFilled(quiverX + i * 36, quiverY, 10, arrowColor);
        drawCircleOutline(quiverX + i * 36, quiverY, 10, {255, 255, 255, 180});
    }

    // Indicador de calibración rápida
    drawText(fontSmall_, "[△ / C] Centrar mira", quiverX, quiverY + 28, {160, 185, 205, 180});

    // 3. Barra de Tensión del Arco (Abajo a la derecha)
    const AimState& aim = input_.getState();
    int barW = 380;
    int barH = 28;
    int barX = 1480;
    int barY = 1010;

    drawText(fontSmall_, "TENSIÓN DEL ARCO (MANTENER R2):", barX, barY - 30, {220, 230, 245, 230});

    // Marco exterior
    SDL_Rect barBg = {barX, barY, barW, barH};
    SDL_SetRenderDrawColor(ren_, 25, 35, 45, 220);
    SDL_RenderFillRect(ren_, &barBg);

    // Relleno progresivo
    int fillW = static_cast<int>(aim.tension * barW);
    if (fillW > 0) {
        SDL_Rect fillRect = {barX, barY, fillW, barH};
        Uint8 r = static_cast<Uint8>(std::min(255.0f, 50.0f + aim.tension * 205.0f));
        Uint8 g = static_cast<Uint8>(std::max(40.0f, 220.0f - aim.tension * 160.0f));
        SDL_SetRenderDrawColor(ren_, r, g, 30, 255);
        SDL_RenderFillRect(ren_, &fillRect);
    }
    SDL_SetRenderDrawColor(ren_, 160, 180, 200, 255);
    SDL_RenderDrawRect(ren_, &barBg);

    // Porcentaje
    std::string pctStr = std::to_string(static_cast<int>(aim.tension * 100.0f)) + "%";
    drawText(fontSmall_, pctStr, barX + barW + 15, barY + 4, {255, 255, 255, 255});
}

void ArcheryGame::renderCalibrationScreen() {
    SDL_Rect modal = {360, 220, 1200, 640};
    SDL_SetRenderDrawColor(ren_, 12, 18, 25, 240);
    SDL_RenderFillRect(ren_, &modal);
    SDL_SetRenderDrawColor(ren_, 60, 120, 180, 255);
    SDL_RenderDrawRect(ren_, &modal);

    drawText(fontLarge_, "CALIBRACIÓN DE APUNTADO", 960, 270, {255, 215, 60, 255}, true);

    drawText(fontMed_, "Sostén el DualShock apuntando cómodamente hacia la pantalla.", 960, 380, {240, 240, 250, 255}, true);
    drawText(fontSmall_, "La orientación física del mando controlará la mira en tiempo real.", 960, 435, {180, 205, 225, 255}, true);

    drawText(fontMed_, "Controles:", 960, 520, {255, 255, 255, 255}, true);
    drawText(fontSmall_, "• Orientación DualShock: Apuntar como arma/arco", 960, 570, {220, 230, 240, 255}, true);
    drawText(fontSmall_, "• Mantener R2: Tensar arco (progresivo)", 960, 610, {220, 230, 240, 255}, true);
    drawText(fontSmall_, "• Soltar R2: Disparar flecha", 960, 650, {220, 230, 240, 255}, true);
    drawText(fontSmall_, "• Triángulo [△] o [C]: Recalibrar centro en cualquier momento", 960, 690, {255, 200, 60, 255}, true);

    drawText(fontLarge_, "Presiona [X] o [ENTER] para Comenzar", 960, 770, {80, 240, 140, 255}, true);
}

void ArcheryGame::renderRoundSummary() {
    SDL_Rect modal = {460, 260, 1000, 540};
    SDL_SetRenderDrawColor(ren_, 15, 22, 30, 245);
    SDL_RenderFillRect(ren_, &modal);
    SDL_SetRenderDrawColor(ren_, 70, 140, 200, 255);
    SDL_RenderDrawRect(ren_, &modal);

    std::string title = "¡RONDA " + std::to_string(currentRound_) + " COMPLETADA!";
    drawText(fontLarge_, title, 960, 310, {255, 215, 60, 255}, true);

    std::string rPts = "Puntos en esta ronda: +" + std::to_string(roundScore_);
    drawText(fontMed_, rPts, 960, 420, {100, 240, 150, 255}, true);

    std::string totalPts = "Puntuación total acumulada: " + std::to_string(totalScore_);
    drawText(fontMed_, totalPts, 960, 480, {240, 245, 255, 255}, true);

    std::string nextDist = "Siguiente distancia: " + std::to_string(static_cast<int>(targetDistance_ + 5.0f)) + " metros";
    drawText(fontSmall_, nextDist, 960, 550, {255, 190, 40, 255}, true);

    drawText(fontMed_, "Presiona [X] para la siguiente ronda", 960, 680, {80, 240, 140, 255}, true);
    drawText(fontSmall_, "Presiona [O] o [ESC] para salir al menú", 960, 730, {180, 190, 200, 200}, true);
}

void ArcheryGame::renderMatchOver() {
    SDL_Rect modal = {420, 180, 1080, 720};
    SDL_SetRenderDrawColor(ren_, 12, 18, 26, 250);
    SDL_RenderFillRect(ren_, &modal);
    SDL_SetRenderDrawColor(ren_, 255, 215, 60, 255);
    SDL_RenderDrawRect(ren_, &modal);

    drawText(fontLarge_, "¡PARTIDA COMPLETADA!", 960, 230, {255, 220, 50, 255}, true);

    if (isNewRecord_) {
        drawText(fontLarge_, "★ ¡NUEVO RÉCORD HISTÓRICO! ★", 960, 310, {255, 70, 70, 255}, true);
    } else {
        std::string recTxt = "Récord actual: " + std::to_string(config_.highRecord) + " puntos";
        drawText(fontSmall_, recTxt, 960, 320, {200, 220, 240, 220}, true);
    }

    std::string finalPts = "PUNTUACIÓN FINAL: " + std::to_string(totalScore_);
    drawText(fontLarge_, finalPts, 960, 400, {100, 245, 140, 255}, true);

    int totalShots = kTotalRounds * kArrowsPerRound;
    float accuracy = totalShots > 0 ? (static_cast<float>(totalHits_) / totalShots) * 100.0f : 0.0f;
    std::string accStr = "Precisión: " + std::to_string(static_cast<int>(accuracy)) + "% (" +
                         std::to_string(totalHits_) + " / " + std::to_string(totalShots) + " flechas en diana)";
    drawText(fontMed_, accStr, 960, 490, {240, 245, 255, 255}, true);

    std::string bullseyeStr = "Dianas Perfectas (Centros): " + std::to_string(totalBullseyes_);
    drawText(fontMed_, bullseyeStr, 960, 550, {255, 215, 60, 255}, true);

    std::string bestShotStr = "Mejor disparo: +" + std::to_string(bestShotPoints_) + " pts";
    drawText(fontMed_, bestShotStr, 960, 610, {220, 230, 240, 255}, true);

    drawText(fontMed_, "Presiona [X] o [ENTER] para Jugar de Nuevo", 960, 730, {80, 240, 140, 255}, true);
    drawText(fontSmall_, "Presiona [O] o [ESC] para Salir al Menú Principal", 960, 785, {180, 190, 200, 220}, true);
}

void ArcheryGame::renderPauseMenu() {
    SDL_Rect modal = {480, 200, 960, 680};
    SDL_SetRenderDrawColor(ren_, 15, 22, 30, 245);
    SDL_RenderFillRect(ren_, &modal);
    SDL_SetRenderDrawColor(ren_, 80, 160, 220, 255);
    SDL_RenderDrawRect(ren_, &modal);

    drawText(fontLarge_, "CONFIGURACIÓN Y PAUSA", 960, 240, {255, 215, 60, 255}, true);
    drawText(fontSmall_, "Usa [Arriba/Abajo] para seleccionar y [Izq/Der] para ajustar", 960, 310, {180, 200, 220, 220}, true);

    std::vector<std::string> options = {
        "Sensibilidad Horizontal: " + std::to_string(config_.sensitivityX).substr(0, 4),
        "Sensibilidad Vertical: " + std::to_string(config_.sensitivityY).substr(0, 4),
        "Suavizado de Mira: " + std::to_string(config_.smoothing).substr(0, 4),
        "Zona Muerta Giroscopio: " + std::to_string(config_.deadzone).substr(0, 4),
        std::string("Invertir Vertical: ") + (config_.invertY ? "SÍ" : "NO"),
        std::string("Vibración Háptica: ") + (config_.rumbleEnabled ? "ACTIVADA" : "DESACTIVADA"),
        "► VOLVER AL JUEGO ◄"
    };

    int startY = 370;
    for (size_t i = 0; i < options.size(); ++i) {
        bool selected = (static_cast<int>(i) == pauseMenuIndex_);
        SDL_Color c = selected ? SDL_Color{80, 240, 140, 255} : SDL_Color{220, 230, 240, 220};
        drawText(fontMed_, options[i], 960, startY + static_cast<int>(i * 55), c, true);
    }

    drawText(fontSmall_, "Presiona [X] o [O] para reanudar", 960, 810, {160, 180, 200, 200}, true);
}
