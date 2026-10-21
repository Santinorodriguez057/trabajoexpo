#include "archery_game.h"
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstdlib>

using namespace ArcheryTheme;

ArcheryGame::ArcheryGame(SDL_Renderer* ren, TTF_Font* fontLarge, TTF_Font* fontMed, TTF_Font* fontSmall, GameConfig& config)
    : ren_(ren), fontLarge_(fontLarge), fontMed_(fontMed), fontSmall_(fontSmall), config_(config), input_(config) {
}

ArcheryGame::~ArcheryGame() {
    if (EXTRA_ENABLE_SAVED_RECORD) {
        config_.save();
    }
}

int ArcheryGame::run() {
    input_.init();
    audio_.init();
    if (EXTRA_ENABLE_SAVED_RECORD) {
        config_.load();
    }

    startRound(1);
    state_ = GameState::CALIBRATION;

    Uint32 lastTime = SDL_GetTicks();

    while (running_) {
        Uint32 currentTime = SDL_GetTicks();
        float dt = (currentTime - lastTime) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f;
        lastTime = currentTime;

        roundTime_ += dt;

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

    // Distancia progresiva de la diana
    float baseDist = 18.0f + (roundNumber - 1) * 5.5f;
    float distVariation = ((rand() % 100) / 100.0f - 0.5f) * 4.0f;
    targetDistance_ = std::clamp(baseDist + distVariation, 15.0f, 75.0f);

    // Variación horizontal y de altura en rondas intermedias y avanzadas
    if (roundNumber > 2) {
        targetX_ = ((rand() % 100) / 100.0f - 0.5f) * 2.0f;
        targetY_ = 1.3f + ((rand() % 100) / 100.0f - 0.5f) * 0.5f;
    } else {
        targetX_ = 0.0f;
        targetY_ = 1.3f;
    }

    targetRadius_ = 0.85f; // Radio visual oficial cómodo

    // Viento aleatorio por ronda (Extra 5)
    if (EXTRA_ENABLE_WIND) {
        float speed = (rand() % 45) / 10.0f; // 0.0 a 4.5 m/s
        int dirSign = (rand() % 2 == 0) ? 1 : -1;
        windX_ = speed * dirSign;
        windY_ = ((rand() % 20) / 10.0f - 1.0f) * 0.4f;
        windSpeed_ = speed;

        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f M/S %s", speed, dirSign > 0 ? "➔" : "⬅");
        windLabel_ = (speed < 0.3f) ? "CALMA (0.0 M/S)" : std::string(buf);
    } else {
        windX_ = 0.0f;
        windY_ = 0.0f;
        windSpeed_ = 0.0f;
        windLabel_ = "SIN VIENTO";
    }
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

        // Control específico de navegación en el Menú de Ajustes (Paso 3)
        if (state_ == GameState::PAUSE_MENU) {
            if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_UP) {
                    pauseMenuIndex_ = (pauseMenuIndex_ + 3) % 4;
                } else if (ev.key.keysym.sym == SDLK_DOWN) {
                    pauseMenuIndex_ = (pauseMenuIndex_ + 1) % 4;
                } else if (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_SPACE) {
                    // Ejecutar opción seleccionada
                    if (pauseMenuIndex_ == 0) {
                        // Continuar
                        state_ = GameState::PLAYING;
                    } else if (pauseMenuIndex_ == 1) {
                        // Calibrar centro
                        input_.calibrate();
                        state_ = GameState::PLAYING;
                    } else if (pauseMenuIndex_ == 2) {
                        // Volver al menú principal
                        running_ = false;
                    } else if (pauseMenuIndex_ == 3) {
                        // Salir
                        running_ = false;
                        std::exit(0);
                    }
                } else if (ev.key.keysym.sym == SDLK_ESCAPE) {
                    state_ = GameState::PLAYING;
                }
            } else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
                if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
                    pauseMenuIndex_ = (pauseMenuIndex_ + 3) % 4;
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
                    pauseMenuIndex_ = (pauseMenuIndex_ + 1) % 4;
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                    // X en DualShock (Confirmar)
                    if (pauseMenuIndex_ == 0) {
                        state_ = GameState::PLAYING;
                    } else if (pauseMenuIndex_ == 1) {
                        input_.calibrate();
                        state_ = GameState::PLAYING;
                    } else if (pauseMenuIndex_ == 2) {
                        running_ = false;
                    } else if (pauseMenuIndex_ == 3) {
                        running_ = false;
                        std::exit(0);
                    }
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_B || 
                           ev.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                    // Círculo o START: Continuar / Cerrar menú
                    state_ = GameState::PLAYING;
                }
            }
        }
    }
}

void ArcheryGame::update(float dt) {
    input_.update(dt);
    const AimState& aim = input_.getState();

    // Gestión del sonido al tensar (sin vibrar para no interferir con el giroscopio)
    if (state_ == GameState::PLAYING && aim.isDrawing) {
        audio_.playDraw(aim.tension);
    }

    // Vibración de la cuerda al soltar
    if (stringVibeTimer_ > 0.0f) {
        stringVibeTimer_ -= dt;
        stringVibration_ = std::sin(stringVibeTimer_ * 60.0f) * (stringVibeTimer_ / 0.35f);
    } else {
        stringVibration_ = 0.0f;
    }

    // Oscilación de impacto en la diana
    if (targetWobble_ > 0.0f) {
        targetWobble_ = std::max(0.0f, targetWobble_ - dt * 3.5f);
    }

    // Amortiguación del temblor de pantalla (Screen Shake)
    if (screenShakeTrauma_ > 0.0f) {
        screenShakeTrauma_ = std::max(0.0f, screenShakeTrauma_ - dt * 2.8f);
    }

    // Actualizar textos flotantes
    for (auto it = floatingTexts_.begin(); it != floatingTexts_.end();) {
        it->life -= dt;
        it->y -= dt * 25.0f; // Suben suavemente
        if (it->life <= 0.0f) {
            it = floatingTexts_.erase(it);
        } else {
            ++it;
        }
    }

    // 2. Máquina de Estados del Juego
    switch (state_) {
        case GameState::CALIBRATION:
            if (input_.confirmPressed()) {
                input_.calibrate();
                state_ = GameState::PLAYING;
            }
            break;

        case GameState::PLAYING:
            // Abrir menú de pausa con START o ESC
            if (input_.pausePressed()) {
                state_ = GameState::PAUSE_MENU;
                pauseMenuIndex_ = 0;
                break;
            }

            // Disparar flecha al soltar R2
            if (aim.justFired && !activeArrow_.active && arrowsRemaining_ > 0) {
                shootArrow(aim.releaseTension);
                if (EXTRA_ENABLE_RUMBLE) {
                    input_.setRumble(0.85f, 1.0f, 130); // Pulso seco al soltar
                }
            }

            // Simulación balística de la flecha en vuelo
            if (activeArrow_.active) {
                SDL_Point pScreen = project3D(activeArrow_.x, activeArrow_.y, activeArrow_.z);
                activeArrow_.trail.push_back({static_cast<float>(pScreen.x), static_cast<float>(pScreen.y)});
                if (activeArrow_.trail.size() > 18) activeArrow_.trail.erase(activeArrow_.trail.begin());

                // Desvío por viento en vuelo (Extra 5)
                if (EXTRA_ENABLE_WIND) {
                    activeArrow_.vx += windX_ * dt * 0.22f;
                    activeArrow_.vy += windY_ * dt * 0.22f;
                }

                // Gravedad
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
                        if (currentRound_ < kTotalRounds) {
                            state_ = GameState::ROUND_SUMMARY;
                            audio_.playFanfare();
                        } else {
                            state_ = GameState::MATCH_OVER;
                            audio_.playFanfare();
                            if (EXTRA_ENABLE_SAVED_RECORD && totalScore_ > config_.highRecord) {
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
            } else if (input_.cancelPressed() || input_.pausePressed()) {
                running_ = false; // Volver al menú
            }
            break;

        case GameState::MATCH_OVER:
            if (input_.confirmPressed()) {
                totalScore_ = 0;
                totalHits_ = 0;
                totalBullseyes_ = 0;
                bestShotPoints_ = 0;
                isNewRecord_ = false;
                startRound(1);
                state_ = GameState::PLAYING;
            } else if (input_.cancelPressed() || input_.pausePressed()) {
                running_ = false;
            }
            break;

        case GameState::PAUSE_MENU:
            break;
    }
}

void ArcheryGame::shootArrow(float tension) {
    audio_.playShoot();
    stringVibeTimer_ = 0.35f;

    const AimState& aim = input_.getState();

    activeArrow_.active = true;
    activeArrow_.trail.clear();

    // El arco ahora está centrado en la vista del arquero
    activeArrow_.x = 0.0f;
    activeArrow_.y = 1.05f;
    activeArrow_.z = 0.6f;

    // Velocidad según tensión (35 a 80 m/s)
    float speed = 35.0f + tension * 45.0f;
    activeArrow_.initialSpeed = speed;

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
        totalHits_++;
        targetWobble_ = 1.0f;

        float rFrac = dist / targetRadius_;
        int pts = 0;
        std::string label;
        SDL_Color color;

        if (rFrac <= 0.10f) {
            pts = 100;
            label = "¡CENTRO PERFECTO! +100";
            color = COLOR_ACCENT_GOLD;
            totalBullseyes_++;
            audio_.playBullseye();
            if (EXTRA_ENABLE_RUMBLE) input_.setRumble(1.0f, 1.0f, 250);
            if (EXTRA_ENABLE_FLOATING_TEXTS) screenShakeTrauma_ = 1.0f;
            input_.setLED(255, 230, 80);
        } else if (rFrac <= 0.20f) {
            pts = 80;
            label = "¡DIANA DORADA! +80";
            color = COLOR_ACCENT_GOLD;
            audio_.playBullseye();
            if (EXTRA_ENABLE_RUMBLE) input_.setRumble(0.7f, 0.9f, 200);
            if (EXTRA_ENABLE_FLOATING_TEXTS) screenShakeTrauma_ = 0.8f;
            input_.setLED(255, 205, 30);
        } else if (rFrac <= 0.40f) {
            pts = (rFrac <= 0.30f) ? 60 : 50;
            label = "ZONA ROJA +" + std::to_string(pts);
            color = COLOR_ACCENT_RED;
            audio_.playImpact();
            if (EXTRA_ENABLE_RUMBLE) input_.setRumble(0.5f, 0.6f, 160);
            if (EXTRA_ENABLE_FLOATING_TEXTS) screenShakeTrauma_ = 0.5f;
        } else if (rFrac <= 0.60f) {
            pts = (rFrac <= 0.50f) ? 40 : 30;
            label = "ZONA AZUL +" + std::to_string(pts);
            color = COLOR_ACCENT_BLUE;
            audio_.playImpact();
            if (EXTRA_ENABLE_RUMBLE) input_.setRumble(0.4f, 0.5f, 140);
            if (EXTRA_ENABLE_FLOATING_TEXTS) screenShakeTrauma_ = 0.35f;
        } else if (rFrac <= 0.80f) {
            pts = (rFrac <= 0.70f) ? 20 : 15;
            label = "ZONA NEGRA +" + std::to_string(pts);
            color = COLOR_TEXT_MUTED;
            audio_.playImpact();
            if (EXTRA_ENABLE_RUMBLE) input_.setRumble(0.3f, 0.4f, 120);
            if (EXTRA_ENABLE_FLOATING_TEXTS) screenShakeTrauma_ = 0.25f;
        } else {
            pts = (rFrac <= 0.90f) ? 10 : 5;
            label = "ZONA BLANCA +" + std::to_string(pts);
            color = COLOR_TEXT_MAIN;
            audio_.playImpact();
            if (EXTRA_ENABLE_RUMBLE) input_.setRumble(0.2f, 0.3f, 100);
            if (EXTRA_ENABLE_FLOATING_TEXTS) screenShakeTrauma_ = 0.15f;
        }

        totalScore_ += pts;
        roundScore_ += pts;
        if (pts > bestShotPoints_) bestShotPoints_ = pts;

        float angle = ((rand() % 60) - 30) * 0.01f;
        stuckArrows_.push_back({diffX, diffY, angle});

        if (EXTRA_ENABLE_FLOATING_TEXTS) {
            floatingTexts_.push_back({label, static_cast<float>(screenHit.x), static_cast<float>(screenHit.y - 70), 2.2f, 2.2f, color, 1.2f});
        }
    } else {
        audio_.playImpact();
        if (EXTRA_ENABLE_RUMBLE) input_.setRumble(0.2f, 0.2f, 80);
        if (EXTRA_ENABLE_FLOATING_TEXTS) {
            floatingTexts_.push_back({"FALLO 0", static_cast<float>(screenHit.x), static_cast<float>(screenHit.y - 40), 1.8f, 1.8f, COLOR_ACCENT_RED, 1.0f});
        }
    }
}

SDL_Point ArcheryGame::project3D(float x, float y, float z) {
    if (z <= 0.1f) z = 0.1f;
    const float fov = 2200.0f;
    int px = 960 + static_cast<int>((x / z) * fov);
    int py = HORIZON_Y - static_cast<int>(((y - 1.3f) / z) * fov);
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
    SDL_Rect dst = {center ? x - w / 2 : x, y - h / 2, w, h};
    SDL_RenderCopy(ren_, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

void ArcheryGame::drawFlat(float cx, float cy, float w, float h, float angle, SDL_Color color) {
    SDL_SetRenderDrawColor(ren_, color.r, color.g, color.b, color.a);
    SDL_Rect r = { static_cast<int>(cx - w / 2.0f), static_cast<int>(cy - h / 2.0f), static_cast<int>(w), static_cast<int>(h) };
    if (std::abs(angle) < 0.1f) {
        SDL_RenderFillRect(ren_, &r);
    } else {
        // Rotación con primitivas de 45°
        float rad = angle * 3.14159f / 180.0f;
        float cosA = std::cos(rad);
        float sinA = std::sin(rad);
        int halfW = static_cast<int>(w / 2.0f);
        int halfH = static_cast<int>(h / 2.0f);
        for (int py = -halfH; py <= halfH; py += 2) {
            int px1 = static_cast<int>(-halfW * cosA - py * sinA);
            int py1 = static_cast<int>(-halfW * sinA + py * cosA);
            int px2 = static_cast<int>(halfW * cosA - py * sinA);
            int py2 = static_cast<int>(halfW * sinA + py * cosA);
            SDL_RenderDrawLine(ren_, static_cast<int>(cx + px1), static_cast<int>(cy + py1),
                                     static_cast<int>(cx + px2), static_cast<int>(cy + py2));
        }
    }
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
    // Cálculo de temblor de pantalla (Screen Shake)
    int shakeX = 0;
    int shakeY = 0;
    if (EXTRA_ENABLE_FLOATING_TEXTS && screenShakeTrauma_ > 0.0f) {
        float shakeMag = screenShakeTrauma_ * screenShakeTrauma_ * 14.0f;
        shakeX = static_cast<int>(std::sin(roundTime_ * 45.0f) * shakeMag);
        shakeY = static_cast<int>(std::cos(roundTime_ * 55.0f) * shakeMag * 0.7f);
    }

    // Fondo base
    SDL_SetRenderDrawColor(ren_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(ren_);

    renderBackground();
    renderTarget();
    renderFlyingArrow();
    renderFirstPersonBow();
    renderHUD();

    // Renderizar textos flotantes de puntuación
    for (const auto& ft : floatingTexts_) {
        float alpha = std::clamp(ft.life / (ft.maxLife * 0.35f), 0.0f, 1.0f);
        if (alpha > 0.05f) {
            int tx = static_cast<int>(ft.x) + shakeX;
            int ty = static_cast<int>(ft.y) + shakeY;
            // Badge flotante estilo brutalista
            int tagW = 280;
            int tagH = 46;
            drawBrutalistBox(ren_, tx - tagW / 2, ty - tagH / 2, tagW, tagH, false, COLOR_CARD_BG, ft.color, 6);
            drawText(fontMed_, ft.text, tx, ty, ft.color, true);
        }
    }

    if (state_ == GameState::CALIBRATION) renderCalibrationScreen();
    else if (state_ == GameState::ROUND_SUMMARY) renderRoundSummary();
    else if (state_ == GameState::MATCH_OVER) renderMatchOver();
    else if (state_ == GameState::PAUSE_MENU) renderPauseMenu();

    SDL_RenderPresent(ren_);
}

void ArcheryGame::renderBackground() {
    // 1. Cielo con Cuadrícula Dinámica (Estilo Duelo de Vaqueros)
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren_, COLOR_GRID.r, COLOR_GRID.g, COLOR_GRID.b, COLOR_GRID.a);
    int gridOffset = static_cast<int>(roundTime_ * 15.0f) % 60;
    for (int x = gridOffset - 60; x < 1920; x += 60) SDL_RenderDrawLine(ren_, x, 0, x, HORIZON_Y);
    for (int y = gridOffset - 60; y < HORIZON_Y; y += 60) SDL_RenderDrawLine(ren_, 0, y, 1920, y);
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_NONE);

    // 2. Sol Centrado con Ondas de Calor (Mirage)
    float heatWave = std::sin(roundTime_ * 5.0f) * 3.5f;
    float sunX = 960.0f;
    float sunY = 230.0f + heatWave;
    float sunSize = 130.0f + std::sin(roundTime_ * 3.0f) * 4.0f;
    drawFlat(sunX, sunY, sunSize, sunSize, 0.0f, COLOR_SUN);
    drawFlat(sunX, sunY, sunSize, sunSize, 45.0f, COLOR_SUN);
    for (int i = 0; i < 12; i++) {
        float a = i * 30.0f + roundTime_ * 4.0f;
        float rad = a * 3.14159f / 180.0f;
        float rayLen = 32.0f + std::sin(roundTime_ * 6.0f + i) * 5.0f;
        drawFlat(sunX + std::cos(rad) * 88.0f, sunY + std::sin(rad) * 88.0f, rayLen, 12.0f, a, COLOR_SUN);
    }

    // 3. Nubes Flotantes Planas
    float cloudDrift = std::fmod(roundTime_ * 8.0f, 2400.0f);
    drawFlat(200.0f + cloudDrift * 0.5f, 130.0f, 140.0f, 26.0f, 0.0f, COLOR_CLOUD);
    drawFlat(220.0f + cloudDrift * 0.5f, 105.0f, 75.0f, 26.0f, 0.0f, COLOR_CLOUD);
    drawFlat(1600.0f - cloudDrift * 0.3f, 220.0f, 180.0f, 32.0f, 0.0f, COLOR_CLOUD);
    drawFlat(1640.0f - cloudDrift * 0.3f, 190.0f, 90.0f, 32.0f, 0.0f, COLOR_CLOUD);
    drawFlat(850.0f + cloudDrift * 0.4f, 150.0f, 110.0f, 26.0f, 0.0f, COLOR_CLOUD);

    // 4. Siluetas de Colinas y Mesas lejanas (Neo-Brutalistas)
    drawFlat(220.0f, 510.0f, 260.0f, 40.0f, 0.0f, COLOR_HILLS);
    drawFlat(190.0f, 475.0f, 170.0f, 35.0f, 0.0f, COLOR_HILLS);
    drawFlat(150.0f, 395.0f, 85.0f, 190.0f, 0.0f, COLOR_HILLS);
    drawFlat(1700.0f, 520.0f, 200.0f, 200.0f, 45.0f, COLOR_HILLS);
    drawFlat(1820.0f, 535.0f, 140.0f, 140.0f, 45.0f, COLOR_HILLS);

    // 5. Suelo Llanura
    SDL_SetRenderDrawColor(ren_, COLOR_GROUND.r, COLOR_GROUND.g, COLOR_GROUND.b, 255);
    SDL_Rect groundRect = {0, HORIZON_Y, 1920, 1080 - HORIZON_Y};
    SDL_RenderFillRect(ren_, &groundRect);

    // 6. Línea de horizonte negra pura brutalista de 4px
    SDL_SetRenderDrawColor(ren_, COLOR_HORIZON.r, COLOR_HORIZON.g, COLOR_HORIZON.b, 255);
    SDL_Rect horizLine = {0, HORIZON_Y - 2, 1920, HORIZON_THICK};
    SDL_RenderFillRect(ren_, &horizLine);

    // 7. Carriles de tiro hacia el horizonte
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren_, 0, 0, 0, 18);
    SDL_RenderDrawLine(ren_, 960, HORIZON_Y, 200, 1080);
    SDL_RenderDrawLine(ren_, 960, HORIZON_Y, 1720, 1080);
    SDL_RenderDrawLine(ren_, 960, HORIZON_Y, 600, 1080);
    SDL_RenderDrawLine(ren_, 960, HORIZON_Y, 1320, 1080);
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_NONE);

    // Banderines de distancia
    std::vector<float> distMarkers = {20.0f, 30.0f, 50.0f, 70.0f};
    for (float d : distMarkers) {
        SDL_Point mP = project3D(-3.0f, 1.3f, d);
        if (mP.y >= HORIZON_Y && mP.y <= 1060) {
            SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
            SDL_RenderDrawLine(ren_, mP.x, mP.y, mP.x, mP.y - 25);
            SDL_RenderDrawLine(ren_, mP.x + 1, mP.y, mP.x + 1, mP.y - 25);
            // Pequeña bandera triangular
            SDL_RenderDrawLine(ren_, mP.x, mP.y - 25, mP.x + 12, mP.y - 20);
            SDL_RenderDrawLine(ren_, mP.x + 12, mP.y - 20, mP.x, mP.y - 15);
            std::string markerTxt = std::to_string(static_cast<int>(d)) + "M";
            drawText(fontSmall_, markerTxt, mP.x + 18, mP.y - 20, COLOR_TEXT_MUTED);
        }
    }
}

void ArcheryGame::renderTarget() {
    float wobbleOffset = std::sin(SDL_GetTicks() * 0.04f) * targetWobble_ * 0.06f;
    SDL_Point center = project3D(targetX_ + wobbleOffset, targetY_, targetDistance_);

    const float fov = 2200.0f;
    int radiusPx = static_cast<int>((targetRadius_ / targetDistance_) * fov);
    if (radiusPx < 6) radiusPx = 6;

    // 1. Patas de madera del trípode (A-Frame con contornos negros brutalistas)
    int legBottomY = center.y + static_cast<int>(radiusPx * 1.7f);
    SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
    for (int t = -1; t <= 1; ++t) {
        SDL_RenderDrawLine(ren_, center.x - static_cast<int>(radiusPx * 0.9f) + t, legBottomY, center.x + t, center.y);
        SDL_RenderDrawLine(ren_, center.x + static_cast<int>(radiusPx * 0.9f) + t, legBottomY, center.x + t, center.y);
        SDL_RenderDrawLine(ren_, center.x + t, legBottomY + 10, center.x + t, center.y);
    }

    // 2. Soporte redondo de paja detrás de la diana
    drawCircleFilled(center.x, center.y, radiusPx + 8, {180, 150, 95, 255});
    drawCircleOutline(center.x, center.y, radiusPx + 8, COLOR_BORDER);
    drawCircleOutline(center.x, center.y, radiusPx + 7, COLOR_BORDER);

    // 3. Anillos oficiales FITA con delineados negros precisos
    // Blanco (1-2)
    drawCircleFilled(center.x, center.y, radiusPx, {245, 245, 250, 255});
    drawCircleOutline(center.x, center.y, radiusPx, COLOR_BORDER);
    drawCircleOutline(center.x, center.y, static_cast<int>(radiusPx * 0.9f), {180, 180, 190, 255});

    // Negro (3-4)
    drawCircleFilled(center.x, center.y, static_cast<int>(radiusPx * 0.8f), {35, 35, 40, 255});
    drawCircleOutline(center.x, center.y, static_cast<int>(radiusPx * 0.8f), COLOR_BORDER);
    drawCircleOutline(center.x, center.y, static_cast<int>(radiusPx * 0.7f), {90, 90, 100, 255});

    // Azul (5-6)
    drawCircleFilled(center.x, center.y, static_cast<int>(radiusPx * 0.6f), {30, 145, 235, 255});
    drawCircleOutline(center.x, center.y, static_cast<int>(radiusPx * 0.6f), COLOR_BORDER);
    drawCircleOutline(center.x, center.y, static_cast<int>(radiusPx * 0.5f), {80, 185, 255, 255});

    // Rojo (7-8)
    drawCircleFilled(center.x, center.y, static_cast<int>(radiusPx * 0.4f), {230, 45, 50, 255});
    drawCircleOutline(center.x, center.y, static_cast<int>(radiusPx * 0.4f), COLOR_BORDER);
    drawCircleOutline(center.x, center.y, static_cast<int>(radiusPx * 0.3f), {255, 95, 100, 255});

    // Oro (9-10)
    drawCircleFilled(center.x, center.y, static_cast<int>(radiusPx * 0.2f), COLOR_ACCENT_GOLD);
    drawCircleOutline(center.x, center.y, static_cast<int>(radiusPx * 0.2f), COLOR_BORDER);
    // Centro X
    drawCircleFilled(center.x, center.y, static_cast<int>(radiusPx * 0.1f), {255, 235, 100, 255});
    drawCircleOutline(center.x, center.y, static_cast<int>(radiusPx * 0.1f), {180, 140, 20, 255});

    // 4. Flechas clavadas en la diana
    for (const auto& sa : stuckArrows_) {
        int arrowX = center.x + static_cast<int>((sa.localX / targetDistance_) * fov);
        int arrowY = center.y - static_cast<int>((sa.localY / targetDistance_) * fov);

        int shaftLen = std::max(12, static_cast<int>(radiusPx * 0.45f));
        int tailX = arrowX + static_cast<int>(std::sin(sa.angle) * shaftLen);
        int tailY = arrowY + static_cast<int>(std::cos(sa.angle) * shaftLen);

        SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
        SDL_RenderDrawLine(ren_, arrowX, arrowY, tailX, tailY);
        SDL_SetRenderDrawColor(ren_, COLOR_ARROW_SHAFT.r, COLOR_ARROW_SHAFT.g, COLOR_ARROW_SHAFT.b, 255);
        SDL_RenderDrawLine(ren_, arrowX + 1, arrowY, tailX + 1, tailY);

        // Plumas rojas
        SDL_SetRenderDrawColor(ren_, COLOR_ARROW_FLETCH.r, COLOR_ARROW_FLETCH.g, COLOR_ARROW_FLETCH.b, 255);
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
            SDL_SetRenderDrawColor(ren_, 15, 15, 15, a);
            SDL_RenderDrawLine(ren_, static_cast<int>(activeArrow_.trail[i].x), static_cast<int>(activeArrow_.trail[i].y),
                                     static_cast<int>(activeArrow_.trail[i+1].x), static_cast<int>(activeArrow_.trail[i+1].y));
        }
    }

    SDL_Point tip = project3D(activeArrow_.x, activeArrow_.y, activeArrow_.z);
    SDL_Point nock = project3D(activeArrow_.x - activeArrow_.vx * 0.015f,
                               activeArrow_.y - activeArrow_.vy * 0.015f,
                               activeArrow_.z - activeArrow_.vz * 0.015f);

    SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
    SDL_RenderDrawLine(ren_, tip.x, tip.y, nock.x, nock.y);
    SDL_RenderDrawLine(ren_, tip.x + 1, tip.y, nock.x + 1, nock.y);
    drawCircleFilled(tip.x, tip.y, 3, COLOR_ARROW_TIP);
}

// =========================================================================
// PASO 4: ARCO EN PRIMERA PERSONA (ABAJO AL CENTRO)
// Brazo de madera, cuerda y flecha reactivos a la tensión de R2
// =========================================================================
void ArcheryGame::renderFirstPersonBow() {
    const AimState& aim = input_.getState();
    float tension = std::clamp(aim.tension, 0.0f, 1.0f);

    // 1. Retícula de apuntado físico (sigue la orientación del DualShock)
    int crossX = 960 + static_cast<int>(aim.x * 750.0f);
    int crossY = HORIZON_Y + static_cast<int>(aim.y * 450.0f);

    int ringRadius = 24 - static_cast<int>(tension * 8.0f); // Se contrae al tensar
    SDL_Color reticleColor = aim.isDrawing ? COLOR_ACCENT_RED : COLOR_BORDER;

    // Retícula brutalista de alto contraste
    drawCircleOutline(crossX, crossY, ringRadius, reticleColor);
    drawCircleOutline(crossX, crossY, ringRadius - 1, reticleColor);
    drawCircleFilled(crossX, crossY, 3, reticleColor);

    // Marcas de cruz
    SDL_SetRenderDrawColor(ren_, reticleColor.r, reticleColor.g, reticleColor.b, 255);
    SDL_RenderDrawLine(ren_, crossX - ringRadius - 8, crossY, crossX - ringRadius + 2, crossY);
    SDL_RenderDrawLine(ren_, crossX + ringRadius - 2, crossY, crossX + ringRadius + 8, crossY);
    SDL_RenderDrawLine(ren_, crossX, crossY - ringRadius - 8, crossX, crossY - ringRadius + 2);
    SDL_RenderDrawLine(ren_, crossX, crossY + ringRadius - 2, crossX, crossY + ringRadius + 14);

    // =========================================================================
    // 2. ARCO EN PRIMERA PERSONA VISTO DESDE EL ARQUERO (GRANDE, VERTICAL Y REALISTA)
    // Sostenido con la mano izquierda al frente, apuntando a la diana
    // =========================================================================
    float bowX = 920.0f + aim.x * 80.0f;
    float bowY = 720.0f + aim.y * 55.0f;

    // Flexión dinámica de las palas con la tensión de R2
    float flexInward = tension * 42.0f; // Se doblan hacia adentro bajo la carga
    float flexFlatten = tension * 32.0f; // Se aplanan los extremos

    // Extremos de las palas del arco (Siyahs)
    float topTipX = bowX + 25.0f - flexInward;
    float topTipY = bowY - 360.0f + flexFlatten;

    float botTipX = bowX + 25.0f - flexInward;
    float botTipY = bowY + 360.0f - flexFlatten;

    // Codos intermedios para la curvatura recurva
    float midTopX = bowX - 45.0f - flexInward * 0.3f;
    float midTopY = bowY - 175.0f;

    float midBotX = bowX - 45.0f - flexInward * 0.3f;
    float midBotY = bowY + 175.0f;

    // Nock de la cuerda (punto de apoyo de la flecha)
    // En reposo está cerca de la empuñadura; al tensar R2 se jala hacia atrás hacia el arquero
    float pullBack = tension * 280.0f;
    float nockX = bowX - 55.0f - pullBack;
    float nockY = bowY + tension * 38.0f + stringVibration_ * 15.0f;

    // --- Sombra del arco en primera persona ---
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren_, 10, 10, 10, 40);
    for (int t = 0; t < 8; ++t) {
        SDL_RenderDrawLine(ren_, static_cast<int>(topTipX + 14), static_cast<int>(topTipY + 14 + t),
                                 static_cast<int>(nockX + 14), static_cast<int>(nockY + 14 + t));
        SDL_RenderDrawLine(ren_, static_cast<int>(botTipX + 14), static_cast<int>(botTipY + 14 + t),
                                 static_cast<int>(nockX + 14), static_cast<int>(nockY + 14 + t));
    }
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_NONE);

    // --- CUERDA DEL ARCO (TopTip -> Nock -> BotTip) ---
    // Sombra sutil de la cuerda
    SDL_SetRenderDrawColor(ren_, 15, 15, 15, 140);
    SDL_RenderDrawLine(ren_, static_cast<int>(topTipX + 2), static_cast<int>(topTipY + 2),
                             static_cast<int>(nockX + 2), static_cast<int>(nockY + 2));
    SDL_RenderDrawLine(ren_, static_cast<int>(botTipX + 2), static_cast<int>(botTipY + 2),
                             static_cast<int>(nockX + 2), static_cast<int>(nockY + 2));

    // Cuerda blanca nítida reforzada
    SDL_SetRenderDrawColor(ren_, COLOR_STRING.r, COLOR_STRING.g, COLOR_STRING.b, 255);
    for (int w = -1; w <= 1; ++w) {
        SDL_RenderDrawLine(ren_, static_cast<int>(topTipX + w), static_cast<int>(topTipY),
                                 static_cast<int>(nockX + w), static_cast<int>(nockY));
        SDL_RenderDrawLine(ren_, static_cast<int>(botTipX + w), static_cast<int>(botTipY),
                                 static_cast<int>(nockX + w), static_cast<int>(nockY));
    }

    // --- FLECHA EN EL ARCO (cuando quedan flechas y no está en vuelo) ---
    if (arrowsRemaining_ > 0 && !activeArrow_.active) {
        float arrowLen = 540.0f;
        float arrowTipX = nockX + arrowLen;
        float arrowTipY = bowY - 6.0f;

        // Vástago de la flecha con contorno negro
        SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
        for (int b = -2; b <= 2; ++b) {
            SDL_RenderDrawLine(ren_, static_cast<int>(nockX), static_cast<int>(nockY + b),
                                     static_cast<int>(arrowTipX), static_cast<int>(arrowTipY + b));
        }
        // Madera clara de la flecha
        SDL_SetRenderDrawColor(ren_, COLOR_ARROW_SHAFT.r, COLOR_ARROW_SHAFT.g, COLOR_ARROW_SHAFT.b, 255);
        for (int b = -1; b <= 1; ++b) {
            SDL_RenderDrawLine(ren_, static_cast<int>(nockX), static_cast<int>(nockY + b),
                                     static_cast<int>(arrowTipX - 12), static_cast<int>(arrowTipY + b));
        }

        // Punta de acero de flecha
        SDL_SetRenderDrawColor(ren_, COLOR_ARROW_TIP.r, COLOR_ARROW_TIP.g, COLOR_ARROW_TIP.b, 255);
        for (int p = 0; p < 14; ++p) {
            int px = static_cast<int>(arrowTipX - 14 + p);
            int pySpan = (14 - p) / 2;
            SDL_RenderDrawLine(ren_, px, static_cast<int>(arrowTipY - pySpan),
                                     px, static_cast<int>(arrowTipY + pySpan));
        }
        drawCircleOutline(static_cast<int>(arrowTipX), static_cast<int>(arrowTipY), 5, COLOR_BORDER);

        // Plumas fletchings rojas y blancas en el culatín (nock)
        SDL_SetRenderDrawColor(ren_, COLOR_ARROW_FLETCH.r, COLOR_ARROW_FLETCH.g, COLOR_ARROW_FLETCH.b, 255);
        for (int p = 0; p < 4; ++p) {
            SDL_RenderDrawLine(ren_, static_cast<int>(nockX + 8 + p * 8), static_cast<int>(nockY - 14 + p * 2),
                                     static_cast<int>(nockX + 24 + p * 8), static_cast<int>(nockY - 2));
            SDL_RenderDrawLine(ren_, static_cast<int>(nockX + 8 + p * 8), static_cast<int>(nockY + 14 - p * 2),
                                     static_cast<int>(nockX + 24 + p * 8), static_cast<int>(nockY + 2));
        }
        // Pluma timonera blanca
        SDL_SetRenderDrawColor(ren_, 255, 255, 255, 255);
        SDL_RenderDrawLine(ren_, static_cast<int>(nockX + 12), static_cast<int>(nockY),
                                 static_cast<int>(nockX + 45), static_cast<int>(nockY));
    }

    // --- BRAZOS DE MADERA DEL ARCO (Pala Superior e Inferior) ---
    // 1. Contorno negro grueso brutalista (8px)
    SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
    for (int t = -6; t <= 6; ++t) {
        // Pala superior
        SDL_RenderDrawLine(ren_, static_cast<int>(bowX + t), static_cast<int>(bowY - 35),
                                 static_cast<int>(midTopX + t), static_cast<int>(midTopY));
        SDL_RenderDrawLine(ren_, static_cast<int>(midTopX + t), static_cast<int>(midTopY),
                                 static_cast<int>(topTipX + t), static_cast<int>(topTipY));
        // Pala inferior
        SDL_RenderDrawLine(ren_, static_cast<int>(bowX + t), static_cast<int>(bowY + 35),
                                 static_cast<int>(midBotX + t), static_cast<int>(midBotY));
        SDL_RenderDrawLine(ren_, static_cast<int>(midBotX + t), static_cast<int>(midBotY),
                                 static_cast<int>(botTipX + t), static_cast<int>(botTipY));
    }

    // 2. Relleno de madera noble oscura
    SDL_SetRenderDrawColor(ren_, COLOR_WOOD_DARK.r, COLOR_WOOD_DARK.g, COLOR_WOOD_DARK.b, 255);
    for (int t = -4; t <= 4; ++t) {
        SDL_RenderDrawLine(ren_, static_cast<int>(bowX + t), static_cast<int>(bowY - 35),
                                 static_cast<int>(midTopX + t), static_cast<int>(midTopY));
        SDL_RenderDrawLine(ren_, static_cast<int>(midTopX + t), static_cast<int>(midTopY),
                                 static_cast<int>(topTipX + t), static_cast<int>(topTipY));

        SDL_RenderDrawLine(ren_, static_cast<int>(bowX + t), static_cast<int>(bowY + 35),
                                 static_cast<int>(midBotX + t), static_cast<int>(midBotY));
        SDL_RenderDrawLine(ren_, static_cast<int>(midBotX + t), static_cast<int>(midBotY),
                                 static_cast<int>(botTipX + t), static_cast<int>(botTipY));
    }

    // 3. Veta de madera clara
    SDL_SetRenderDrawColor(ren_, COLOR_WOOD_LIGHT.r, COLOR_WOOD_LIGHT.g, COLOR_WOOD_LIGHT.b, 255);
    for (int t = -1; t <= 1; ++t) {
        SDL_RenderDrawLine(ren_, static_cast<int>(bowX + t), static_cast<int>(bowY - 35),
                                 static_cast<int>(midTopX + t), static_cast<int>(midTopY));
        SDL_RenderDrawLine(ren_, static_cast<int>(midTopX + t), static_cast<int>(midTopY),
                                 static_cast<int>(topTipX + t), static_cast<int>(topTipY));

        SDL_RenderDrawLine(ren_, static_cast<int>(bowX + t), static_cast<int>(bowY + 35),
                                 static_cast<int>(midBotX + t), static_cast<int>(midBotY));
        SDL_RenderDrawLine(ren_, static_cast<int>(midBotX + t), static_cast<int>(midBotY),
                                 static_cast<int>(botTipX + t), static_cast<int>(botTipY));
    }

    // 4. Extremos curvos reforzados (Siyahs de madera dura)
    drawCircleFilled(static_cast<int>(topTipX), static_cast<int>(topTipY), 8, COLOR_BORDER);
    drawCircleFilled(static_cast<int>(topTipX), static_cast<int>(topTipY), 5, COLOR_ACCENT_GOLD);
    drawCircleFilled(static_cast<int>(botTipX), static_cast<int>(botTipY), 8, COLOR_BORDER);
    drawCircleFilled(static_cast<int>(botTipX), static_cast<int>(botTipY), 5, COLOR_ACCENT_GOLD);

    // 5. Empuñadura central acolchada (Riser con cuero negro y ribetes dorados)
    drawBrutalistBox(ren_, static_cast<int>(bowX - 22), static_cast<int>(bowY - 42), 44, 84, false, COLOR_BOW_GRIP, COLOR_ACCENT_GOLD, 6);

    // Cintas doradas de la empuñadura
    SDL_SetRenderDrawColor(ren_, COLOR_ACCENT_GOLD.r, COLOR_ACCENT_GOLD.g, COLOR_ACCENT_GOLD.b, 255);
    SDL_Rect band1 = { static_cast<int>(bowX - 18), static_cast<int>(bowY - 24), 36, 4 };
    SDL_Rect band2 = { static_cast<int>(bowX - 18), static_cast<int>(bowY + 20), 36, 4 };
    SDL_RenderFillRect(ren_, &band1);
    SDL_RenderFillRect(ren_, &band2);
}

// =========================================================================
// PASO 2: HUD NEO-BRUTALISTA COMPLETO
// (RONDA, PUNTOS, RÉCORD, DISTANCIA, FLECHAS, TENSIÓN R2)
// =========================================================================
void ArcheryGame::renderHUD() {
    // 1. Tarjeta Superior Izquierda: RONDA
    drawBrutalistBox(ren_, 40, 24, 260, 68, false, COLOR_CARD_BG, COLOR_ACCENT_BLUE);
    std::string roundStr = "RONDA " + std::to_string(currentRound_) + " / " + std::to_string(kTotalRounds);
    drawText(fontMed_, roundStr, 170, 58, COLOR_TEXT_MAIN, true);

    // 2. Tarjeta Superior Central: DISTANCIA Y VIENTO
    drawBrutalistBox(ren_, 740, 24, 440, 68, false, COLOR_CARD_BG, COLOR_ACCENT_GOLD);
    std::string distStr = "DISTANCIA: " + std::to_string(static_cast<int>(targetDistance_)) + " METROS";
    drawText(fontMed_, distStr, 960, 58, COLOR_TEXT_MAIN, true);

    // Indicador de Viento debajo de la distancia (Extra 5)
    if (EXTRA_ENABLE_WIND) {
        drawBrutalistBox(ren_, 830, 98, 260, 36, false, COLOR_CARD_MUTED, {0,0,0,0}, 4);
        std::string wStr = "VIENTO: " + windLabel_;
        drawText(fontSmall_, wStr, 960, 116, (windSpeed_ > 2.0f) ? COLOR_ACCENT_RED : COLOR_TEXT_MAIN, true);
    }

    // 3. Tarjeta Superior Derecha: PUNTOS Y RÉCORD
    drawBrutalistBox(ren_, 1440, 24, 440, 68, false, COLOR_CARD_BG, COLOR_ACCENT_GREEN);
    std::string scoreStr = "PUNTOS: " + std::to_string(totalScore_);
    drawText(fontMed_, scoreStr, 1550, 58, COLOR_TEXT_MAIN, true);

    // Badge de récord
    std::string recordStr = "RÉCORD: " + std::to_string(config_.highRecord);
    drawText(fontSmall_, recordStr, 1770, 58, COLOR_ACCENT_RED, true);

    // 4. Tarjeta Inferior Izquierda: FLECHAS RESTANTES
    drawBrutalistBox(ren_, 40, 940, 380, 100, false, COLOR_CARD_BG, COLOR_ACCENT_GOLD);
    drawText(fontSmall_, "FLECHAS RESTANTES:", 230, 962, COLOR_TEXT_MAIN, true);

    int quiverStartX = 110;
    for (int i = 0; i < kArrowsPerRound; ++i) {
        int ax = quiverStartX + i * 44;
        int ay = 998;
        if (i < arrowsRemaining_) {
            drawCircleFilled(ax, ay, 12, COLOR_ACCENT_GREEN);
            drawCircleOutline(ax, ay, 12, COLOR_BORDER);
            drawCircleOutline(ax, ay, 11, COLOR_BORDER);
            // Punta interior
            drawCircleFilled(ax, ay, 4, {255, 255, 255, 255});
        } else {
            drawCircleFilled(ax, ay, 12, {200, 200, 205, 255});
            drawCircleOutline(ax, ay, 12, COLOR_BORDER);
        }
    }
    drawText(fontSmall_, "[START/ESC] AJUSTES  •  [△/C] CENTRAR", 230, 1025, COLOR_TEXT_MUTED, true);

    // 5. Tarjeta Inferior Derecha: TENSIÓN R2
    const AimState& aim = input_.getState();
    drawBrutalistBox(ren_, 1500, 940, 380, 100, false, COLOR_CARD_BG, COLOR_ACCENT_RED);

    int pct = static_cast<int>(aim.tension * 100.0f);
    std::string tensStr = "TENSIÓN R2: " + std::to_string(pct) + "%";
    drawText(fontSmall_, tensStr, 1690, 962, COLOR_TEXT_MAIN, true);

    // Barra de progreso brutalista
    int barW = 320;
    int barH = 24;
    int barX = 1530;
    int barY = 986;

    // Fondo gris claro de la barra con borde negro
    SDL_SetRenderDrawColor(ren_, COLOR_CARD_MUTED.r, COLOR_CARD_MUTED.g, COLOR_CARD_MUTED.b, 255);
    SDL_Rect barBg = { barX, barY, barW, barH };
    SDL_RenderFillRect(ren_, &barBg);

    // Relleno progresivo
    int fillW = static_cast<int>(aim.tension * barW);
    if (fillW > 0) {
        SDL_Rect barFill = { barX, barY, fillW, barH };
        SDL_Color fillCol;
        if (aim.tension < 0.5f) {
            fillCol = COLOR_ACCENT_GREEN;
        } else if (aim.tension < 0.85f) {
            fillCol = COLOR_ACCENT_GOLD;
        } else {
            fillCol = COLOR_ACCENT_RED;
        }
        SDL_SetRenderDrawColor(ren_, fillCol.r, fillCol.g, fillCol.b, 255);
        SDL_RenderFillRect(ren_, &barFill);
    }

    // Borde negro de la barra
    SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
    for (int b = 0; b < 2; ++b) {
        SDL_Rect bor = { barX + b, barY + b, barW - b * 2, barH - b * 2 };
        SDL_RenderDrawRect(ren_, &bor);
    }
}

// =========================================================================
// PASO 3: MENÚ DE AJUSTES (START/ESC)
// Opciones: Continuar, Calibrar centro, Volver al menú, Salir
// =========================================================================
void ArcheryGame::renderPauseMenu() {
    // Oscurecimiento de fondo
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren_, 15, 15, 18, 180);
    SDL_Rect fullScreen = { 0, 0, 1920, 1080 };
    SDL_RenderFillRect(ren_, &fullScreen);
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_NONE);

    int cardW = 740;
    int cardH = 460;
    int cardX = 960 - cardW / 2;
    int cardY = 540 - cardH / 2;

    drawBrutalistBox(ren_, cardX, cardY, cardW, cardH, false, COLOR_CARD_BG, COLOR_ACCENT_GOLD, 16);

    drawText(fontLarge_, "MENÚ DE AJUSTES", 960, cardY + 55, COLOR_TEXT_MAIN, true);
    drawText(fontSmall_, "USA [ARRIBA / ABAJO] Y [X / ENTER]", 960, cardY + 105, COLOR_TEXT_MUTED, true);

    std::vector<std::string> options = {
        "CONTINUAR",
        "CALIBRAR CENTRO",
        "VOLVER AL MENÚ",
        "SALIR"
    };

    int startY = cardY + 165;
    for (size_t i = 0; i < options.size(); ++i) {
        bool selected = (static_cast<int>(i) == pauseMenuIndex_);
        int optY = startY + static_cast<int>(i * 60);

        if (selected) {
            // Tarjeta seleccionada con borde grueso y acento dorado
            int tagW = 460;
            int tagH = 48;
            int tagX = 960 - tagW / 2;
            drawBrutalistBox(ren_, tagX, optY - tagH / 2, tagW, tagH, false, COLOR_CARD_MUTED, COLOR_ACCENT_GOLD, 6);
            std::string selText = "►  " + options[i] + "  ◄";
            drawText(fontMed_, selText, 960, optY, COLOR_TEXT_MAIN, true);
        } else {
            drawText(fontMed_, options[i], 960, optY, COLOR_TEXT_MUTED, true);
        }
    }

    drawText(fontSmall_, "[O / ESC] Volver al juego", 960, cardY + cardH - 35, COLOR_TEXT_MUTED, true);
}

void ArcheryGame::renderCalibrationScreen() {
    int cardW = 1000;
    int cardH = 580;
    int cardX = 960 - cardW / 2;
    int cardY = 540 - cardH / 2;

    drawBrutalistBox(ren_, cardX, cardY, cardW, cardH, false, COLOR_CARD_BG, COLOR_ACCENT_GOLD, 16);

    drawText(fontLarge_, "CALIBRACIÓN DE APUNTADO", 960, cardY + 60, COLOR_TEXT_MAIN, true);

    drawText(fontMed_, "Sostén el DualShock apuntando hacia el centro de la pantalla.", 960, cardY + 160, COLOR_TEXT_MAIN, true);
    drawText(fontSmall_, "La orientación del mando controlará la mira con precisión en tiempo real.", 960, cardY + 210, COLOR_TEXT_MUTED, true);

    // Cuadro de controles
    int boxW = 820;
    int boxH = 180;
    int boxX = 960 - boxW / 2;
    int boxY = cardY + 260;
    drawBrutalistBox(ren_, boxX, boxY, boxW, boxH, false, COLOR_CARD_MUTED, {0,0,0,0}, 6);

    drawText(fontSmall_, "• Orientación DualShock: Mover el arco y apuntar a la diana", 960, boxY + 35, COLOR_TEXT_MAIN, true);
    drawText(fontSmall_, "• Mantener R2: Tensar la cuerda del arco progresivamente", 960, boxY + 75, COLOR_TEXT_MAIN, true);
    drawText(fontSmall_, "• Soltar R2: Disparar la flecha", 960, boxY + 115, COLOR_TEXT_MAIN, true);
    drawText(fontSmall_, "• Triángulo [△] o tecla [C]: Recentrar mira en cualquier momento", 960, boxY + 150, COLOR_ACCENT_RED, true);

    drawText(fontLarge_, "PRESIONA [X] O [ENTER] PARA COMENZAR", 960, cardY + cardH - 50, COLOR_ACCENT_GREEN, true);
}

void ArcheryGame::renderRoundSummary() {
    int cardW = 900;
    int cardH = 500;
    int cardX = 960 - cardW / 2;
    int cardY = 540 - cardH / 2;

    drawBrutalistBox(ren_, cardX, cardY, cardW, cardH, false, COLOR_CARD_BG, COLOR_ACCENT_GOLD, 16);

    std::string title = "¡RONDA " + std::to_string(currentRound_) + " COMPLETADA!";
    drawText(fontLarge_, title, 960, cardY + 65, COLOR_TEXT_MAIN, true);

    std::string rPts = "Puntos en esta ronda: +" + std::to_string(roundScore_);
    drawText(fontMed_, rPts, 960, cardY + 160, COLOR_ACCENT_GREEN, true);

    std::string totalPts = "Puntuación acumulada: " + std::to_string(totalScore_) + " PUNTOS";
    drawText(fontMed_, totalPts, 960, cardY + 225, COLOR_TEXT_MAIN, true);

    std::string nextDist = "Siguiente distancia: " + std::to_string(static_cast<int>(targetDistance_ + 5.0f)) + " METROS";
    drawText(fontSmall_, nextDist, 960, cardY + 290, COLOR_ACCENT_GOLD, true);

    drawText(fontMed_, "Presiona [X / ENTER] para Siguiente Ronda", 960, cardY + 390, COLOR_ACCENT_GREEN, true);
    drawText(fontSmall_, "[O / ESC] Volver al Menú Principal", 960, cardY + 445, COLOR_TEXT_MUTED, true);
}

// =========================================================================
// PASO 5: PANTALLA DE RESUMEN TRAS LAS 10 RONDAS
// =========================================================================
void ArcheryGame::renderMatchOver() {
    int cardW = 980;
    int cardH = 640;
    int cardX = 960 - cardW / 2;
    int cardY = 540 - cardH / 2;

    drawBrutalistBox(ren_, cardX, cardY, cardW, cardH, false, COLOR_CARD_BG, isNewRecord_ ? COLOR_ACCENT_RED : COLOR_ACCENT_GOLD, 16);

    drawText(fontLarge_, "¡PARTIDA COMPLETADA!", 960, cardY + 55, COLOR_TEXT_MAIN, true);

    if (isNewRecord_) {
        drawText(fontLarge_, "★ ¡NUEVO RÉCORD HISTÓRICO! ★", 960, cardY + 120, COLOR_ACCENT_RED, true);
    } else {
        std::string recTxt = "RÉCORD HISTÓRICO: " + std::to_string(config_.highRecord) + " PUNTOS";
        drawText(fontSmall_, recTxt, 960, cardY + 120, COLOR_TEXT_MUTED, true);
    }

    // Puntuación Final Destacada
    int scoreTagW = 600;
    int scoreTagH = 70;
    drawBrutalistBox(ren_, 960 - scoreTagW / 2, cardY + 165, scoreTagW, scoreTagH, false, COLOR_CARD_MUTED, COLOR_ACCENT_GREEN, 6);
    std::string finalPts = "PUNTUACIÓN FINAL: " + std::to_string(totalScore_);
    drawText(fontLarge_, finalPts, 960, cardY + 200, COLOR_TEXT_MAIN, true);

    int totalShots = kTotalRounds * kArrowsPerRound;
    float accuracy = totalShots > 0 ? (static_cast<float>(totalHits_) / totalShots) * 100.0f : 0.0f;
    std::string accStr = "Precisión: " + std::to_string(static_cast<int>(accuracy)) + "% (" +
                         std::to_string(totalHits_) + " / " + std::to_string(totalShots) + " flechas en diana)";
    drawText(fontMed_, accStr, 960, cardY + 295, COLOR_TEXT_MAIN, true);

    std::string bullseyeStr = "Dianas Perfectas (X-Ring): " + std::to_string(totalBullseyes_);
    drawText(fontMed_, bullseyeStr, 960, cardY + 360, COLOR_ACCENT_GOLD, true);

    std::string bestShotStr = "Mejor disparo: +" + std::to_string(bestShotPoints_) + " PTS";
    drawText(fontMed_, bestShotStr, 960, cardY + 420, COLOR_TEXT_MAIN, true);

    drawText(fontMed_, "Presiona [X / ENTER] para Jugar de Nuevo", 960, cardY + 520, COLOR_ACCENT_GREEN, true);
    drawText(fontSmall_, "Presiona [O / ESC] para Salir al Menú Principal", 960, cardY + 575, COLOR_TEXT_MUTED, true);
}
