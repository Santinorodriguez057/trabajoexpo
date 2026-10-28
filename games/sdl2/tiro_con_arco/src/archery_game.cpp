#include "archery_game.h"
#include <cmath>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

using namespace ArcheryTheme;

namespace {
struct RoundDifficulty {
    float shotSpeed;
    int movement; // 0 fijo, 1 horizontal, 2 vertical, 3 combinado
    float moveAmplitude;
    float moveRate;
    float hidePeriod;
    int badChance;
    int goldChance;
    float goldLifetime;
};

// Tabla única para ajustar el ritmo y las mecánicas de las diez rondas.
constexpr RoundDifficulty kRoundDifficulty[10] = {
    {225.0f, 0,   0.0f, 0.0f, 0.0f,   0,  0, 0.0f},
    {245.0f, 0,   0.0f, 0.0f, 0.0f,   0,  5, 0.0f},
    {270.0f, 1,  34.0f, 1.0f, 0.0f,   5,  8, 0.0f},
    {295.0f, 1,  48.0f, 1.3f, 0.0f,   8, 10, 0.0f},
    {320.0f, 2,  36.0f, 1.2f, 0.0f,  10, 12, 0.0f},
    {345.0f, 2,  50.0f, 1.5f, 0.0f,  12, 14, 0.0f},
    {370.0f, 3,  42.0f, 1.7f, 2.8f,  14, 16, 3.2f},
    {400.0f, 3,  54.0f, 1.9f, 2.5f,  16, 18, 2.8f},
    {430.0f, 3,  66.0f, 2.1f, 2.2f,  18, 20, 2.4f},
    {465.0f, 3,  78.0f, 2.3f, 1.9f,  20, 22, 2.0f}
};
}

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

        if (state_ == GameState::PLAYING) roundTime_ += dt;

        handleEvents();
        update(dt);
        render();

        SDL_Delay(8);
    }

    return 0;
}

void ArcheryGame::startRound(int roundNumber) {
    currentRound_ = roundNumber;
    roundTime_ = 0.0f;
    hudEntryTimer_ = 0.55f;
    shotsRemaining_ = kShotsPerRound;
    roundScore_ = 0;
    targetsDownThisRound_ = 0;
    comboHits_ = 0;
    targetImpacts_.clear();
    missMarks_.clear();
    activeBullet_.active = false;
    roundEnding_ = false;
    roundEndTimer_ = 0.0f;
    for (int i = 0; i < kTargetCount; ++i) {
        targetFallAngle_[i] = targetFallVelocity_[i] = 0.0f;
        targetFallen_[i] = false;
    }

    // Distancia progresiva de la diana
    float baseDist = 14.0f + (roundNumber - 1) * 2.0f;
    float distVariation = ((rand() % 100) / 100.0f - 0.5f) * 2.0f;
    targetDistance_ = std::clamp(baseDist + distVariation, 12.0f, 34.0f);

    // Cuatro formaciones de feria alineadas con cada uno de los 5 carriles del puesto.
    static const float formations[4][kTargetCount] = {
        {-450.0f, -225.0f, 0.0f, 225.0f, 450.0f},
        {-460.0f, -215.0f, 10.0f, 235.0f, 440.0f},
        {-440.0f, -235.0f, -10.0f, 215.0f, 460.0f},
        {-455.0f, -220.0f, 0.0f, 230.0f, 445.0f}
    };
    static const float heights[4][kTargetCount] = {
        {-28.0f, 18.0f, -12.0f, 24.0f, -22.0f},
        {22.0f, -28.0f, 26.0f, -14.0f, 30.0f},
        {-16.0f, 28.0f, -26.0f, 14.0f, -30.0f},
        {30.0f, -12.0f, 12.0f, -28.0f, 20.0f}
    };
    const int formation = (roundNumber - 1) % 4;
    for (int i = 0; i < kTargetCount; ++i) {
        targetBaseOffsetX_[i] = formations[formation][i] + static_cast<float>(rand() % 17 - 8);
        targetBaseOffsetY_[i] = heights[formation][i] + static_cast<float>(rand() % 13 - 6);
        targetScreenOffsetX_[i] = targetBaseOffsetX_[i];
        targetScreenOffsetY_[i] = targetBaseOffsetY_[i];
        const RoundDifficulty& difficulty = kRoundDifficulty[roundNumber - 1];
        const int roll = rand() % 100;
        targetKind_[i] = roll < difficulty.badChance ? 1 : (roll < difficulty.badChance + difficulty.goldChance ? 2 : 0);
        targetVisible_[i] = true;
    }
    // Siempre debe existir al menos un blanco que permita avanzar sin pegarle a una X.
    bool hasPrizeTarget = false;
    for (int i = 0; i < kTargetCount; ++i)
        hasPrizeTarget = hasPrizeTarget || targetKind_[i] != 1;
    if (!hasPrizeTarget) targetKind_[0] = 0;
    targetX_ = roundNumber > 2 ? ((rand() % 100) / 100.0f - 0.5f) * 1.1f : 0.0f;
    targetY_ = roundNumber > 2 ? 1.3f + ((rand() % 100) / 100.0f - 0.5f) * 0.30f : 1.3f;

    targetRadius_ = 0.34f * targetDistance_ / 18.0f;
    weaponSwayX_ = 0.0f;

    // Viento aleatorio por ronda (Extra 5)
}

void ArcheryGame::restartMatch() {
    totalScore_ = 0;
    totalHits_ = 0;
    totalBullseyes_ = 0;
    bestShotPoints_ = 0;
    isNewRecord_ = false;
    floatingTexts_.clear();
    confetti_.clear();
    input_.recenterAim();
    startRound(1);
    state_ = GameState::PLAYING;
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
                    pauseMenuIndex_ = (pauseMenuIndex_ + 6) % 7;
                } else if (ev.key.keysym.sym == SDLK_DOWN) {
                    pauseMenuIndex_ = (pauseMenuIndex_ + 1) % 7;
                } else if (ev.key.keysym.sym == SDLK_LEFT || ev.key.keysym.sym == SDLK_RIGHT) {
                    const float direction = ev.key.keysym.sym == SDLK_RIGHT ? 1.0f : -1.0f;
                    if (pauseMenuIndex_ == 1) {
                        config_.sensitivityX = config_.sensitivityY = std::clamp(config_.sensitivityX + direction * 0.1f, 0.5f, 3.0f);
                        config_.save();
                    } else if (pauseMenuIndex_ == 2) {
                        config_.deadzone = std::clamp(config_.deadzone + direction * 0.005f, 0.01f, 0.25f);
                        config_.save();
                    }
                } else if (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_SPACE) {
                    // Ejecutar opción seleccionada
                    if (pauseMenuIndex_ == 0) {
                        // Continuar
                        state_ = GameState::PLAYING;
                    } else if (pauseMenuIndex_ == 1) {
                        config_.sensitivityX = config_.sensitivityY = std::min(3.0f, config_.sensitivityX + 0.1f);
                        config_.save();
                    } else if (pauseMenuIndex_ == 2) {
                        config_.deadzone = std::min(0.25f, config_.deadzone + 0.005f);
                        config_.save();
                    } else if (pauseMenuIndex_ == 3) {
                        input_.calibrate();
                        state_ = GameState::PLAYING;
                    } else if (pauseMenuIndex_ == 4) {
                        restartMatch();
                    } else if (pauseMenuIndex_ == 5) {
                        // Volver al menú principal
                        running_ = false;
                    } else if (pauseMenuIndex_ == 6) {
                        // Salir
                        running_ = false;
                    }
                } else if (ev.key.keysym.sym == SDLK_ESCAPE) {
                    state_ = GameState::PLAYING;
                }
            } else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
                if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
                    pauseMenuIndex_ = (pauseMenuIndex_ + 6) % 7;
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
                    pauseMenuIndex_ = (pauseMenuIndex_ + 1) % 7;
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT || ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
                    const float direction = ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT ? 1.0f : -1.0f;
                    if (pauseMenuIndex_ == 1) {
                        config_.sensitivityX = config_.sensitivityY = std::clamp(config_.sensitivityX + direction * 0.1f, 0.5f, 3.0f);
                        config_.save();
                    } else if (pauseMenuIndex_ == 2) {
                        config_.deadzone = std::clamp(config_.deadzone + direction * 0.005f, 0.01f, 0.25f);
                        config_.save();
                    }
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                    // X en DualShock (Confirmar)
                    if (pauseMenuIndex_ == 0) {
                        state_ = GameState::PLAYING;
                    } else if (pauseMenuIndex_ == 1) {
                        config_.sensitivityX = config_.sensitivityY = std::min(3.0f, config_.sensitivityX + 0.1f);
                        config_.save();
                    } else if (pauseMenuIndex_ == 2) {
                        config_.deadzone = std::min(0.25f, config_.deadzone + 0.005f);
                        config_.save();
                    } else if (pauseMenuIndex_ == 3) {
                        input_.calibrate();
                        state_ = GameState::PLAYING;
                    } else if (pauseMenuIndex_ == 4) {
                        restartMatch();
                    } else if (pauseMenuIndex_ == 5) {
                        running_ = false;
                    } else if (pauseMenuIndex_ == 6) {
                        running_ = false;
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
    const float swaySmooth = 1.0f - std::exp(-dt * 7.0f);
    const float idleSway = std::sin(roundTime_ * 1.7f) * 3.0f;
    weaponSwayX_ += (aim.x * 30.0f + idleSway - weaponSwayX_) * swaySmooth;

    if (muzzleFlashTimer_ > 0.0f) {
        muzzleFlashTimer_ = std::max(0.0f, muzzleFlashTimer_ - dt);
    }
    shotRecoil_ = std::max(0.0f, shotRecoil_ - dt * 5.5f);
    reticlePulseTimer_ = std::max(0.0f, reticlePulseTimer_ - dt * 3.0f);
    screenFlashTimer_ = std::max(0.0f, screenFlashTimer_ - dt);
    activeBullet_.impactFlash = std::max(0.0f, activeBullet_.impactFlash - dt);

    // Caída con gravedad y rebote amortiguado de los blancos derribados.
    const RoundDifficulty& difficulty = kRoundDifficulty[std::clamp(currentRound_ - 1, 0, 9)];
    for (int i = 0; i < kTargetCount; ++i) {
        if (!targetFallen_[i]) {
            const float phase = roundTime_ * difficulty.moveRate + i * 1.37f;
            targetScreenOffsetX_[i] = targetBaseOffsetX_[i];
            targetScreenOffsetY_[i] = targetBaseOffsetY_[i];
            if (difficulty.movement == 1 || difficulty.movement == 3)
                targetScreenOffsetX_[i] += std::sin(phase) * difficulty.moveAmplitude;
            if (difficulty.movement == 2 || difficulty.movement == 3)
                targetScreenOffsetY_[i] += std::sin(phase * 0.83f) * difficulty.moveAmplitude * 0.62f;
            targetVisible_[i] = (difficulty.hidePeriod <= 0.0f ||
                 std::fmod(roundTime_ + i * 0.41f, difficulty.hidePeriod) < difficulty.hidePeriod * 0.68f);
            continue;
        }
        // Una lata derribada queda fija en el punto del impacto durante su caída.
        targetVisible_[i] = true;
        targetFallVelocity_[i] = std::min(240.0f, targetFallVelocity_[i] + 450.0f * dt);
        targetFallAngle_[i] = std::min(90.0f, targetFallAngle_[i] + targetFallVelocity_[i] * dt);
        if (targetFallAngle_[i] >= 90.0f) targetFallVelocity_[i] = 0.0f;
    }

    // Amortiguación del temblor de pantalla (Screen Shake)
    if (screenShakeTrauma_ > 0.0f) {
        screenShakeTrauma_ = std::max(0.0f, screenShakeTrauma_ - dt * 2.8f);
    }
    fairCelebrationTimer_ = std::max(0.0f, fairCelebrationTimer_ - dt);
    hudEntryTimer_ = std::max(0.0f, hudEntryTimer_ - dt);
    scorePulseTimer_ = std::max(0.0f, scorePulseTimer_ - dt);
    roundBannerTimer_ = std::max(0.0f, roundBannerTimer_ - dt);

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
    for (auto it = missMarks_.begin(); it != missMarks_.end();) {
        it->life -= dt;
        if (it->life <= 0.0f) it = missMarks_.erase(it);
        else ++it;
    }
    for (auto it = confetti_.begin(); it != confetti_.end();) {
        it->life -= dt;
        it->vy += 220.0f * dt;
        it->x += it->vx * dt;
        it->y += it->vy * dt;
        if (it->life <= 0.0f || it->y > 1120.0f) it = confetti_.erase(it);
        else ++it;
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

            // Un disparo por cada pulsación del gatillo.
            if (aim.justFired && !activeBullet_.active && shotsRemaining_ > 0) {
                shootGun();
                if (EXTRA_ENABLE_RUMBLE) {
                    input_.setRumble(0.85f, 1.0f, 130); // Pulso seco del disparo
                }
            }

            // El proyectil recorre el carril y consume un disparo al llegar al puesto.
            if (activeBullet_.active) {
                SDL_Point pScreen = project3D(activeBullet_.x, activeBullet_.y, activeBullet_.z);
                activeBullet_.trail.push_back({static_cast<float>(pScreen.x), static_cast<float>(pScreen.y)});
                if (activeBullet_.trail.size() > 18) activeBullet_.trail.erase(activeBullet_.trail.begin());

                const float oldX = activeBullet_.x;
                const float oldY = activeBullet_.y;
                const float oldZ = activeBullet_.z;
                activeBullet_.x += activeBullet_.vx * dt;
                activeBullet_.y += activeBullet_.vy * dt;
                activeBullet_.z += activeBullet_.vz * dt;

                // Impacto en el plano de la diana
                if (activeBullet_.z >= targetDistance_) {
                    const float travel = (targetDistance_ - oldZ) / std::max(0.0001f, activeBullet_.z - oldZ);
                    const float impactX = oldX + (activeBullet_.x - oldX) * travel;
                    const float impactY = oldY + (activeBullet_.y - oldY) * travel;
                    activeBullet_.x = impactX;
                    activeBullet_.y = impactY;
                    activeBullet_.z = targetDistance_;
                    const SDL_Point impactScreen = project3D(impactX, impactY, targetDistance_);
                    activeBullet_.impactPoint = {static_cast<float>(impactScreen.x), static_cast<float>(impactScreen.y)};
                    activeBullet_.impactFlash = 0.16f;
                    evaluateImpact(impactX, impactY);
                    activeBullet_.active = false;
                    // Cada disparo consume una bala, tanto al acertar como al fallar.
                    shotsRemaining_ = std::max(0, shotsRemaining_ - 1);

                    bool noPrizeTargetsRemain = true;
                    for (int i = 0; i < kTargetCount; ++i) {
                        if (!targetFallen_[i] && targetKind_[i] != 1) {
                            noPrizeTargetsRemain = false;
                            break;
                        }
                    }
                    // Iniciar el retraso de fin de ronda
                    // Si se quedan sin balas: transición casi instantánea (0.05 s)
                    // Si se derribó la última lata: esperar la animación de caída (0.72 s)
                    if (!roundEnding_ && (shotsRemaining_ <= 0 || noPrizeTargetsRemain)) {
                        roundEnding_ = true;
                        const bool outOfAmmo = (shotsRemaining_ <= 0 && !noPrizeTargetsRemain);
                        roundEndTimer_ = outOfAmmo ? 0.05f : 0.72f;
                        input_.recenterAim();
                        weaponSwayX_ = 0.0f;
                        if (targetsDownThisRound_ == kTargetCount) {
                            const int cleanBonus = 100 + (currentRound_ - 1) * 50;
                            totalScore_ += cleanBonus;
                            roundScore_ += cleanBonus;
                            fairCelebrationTimer_ = 1.8f;
                            floatingTexts_.push_back({"PUESTO LIMPIO +" + std::to_string(cleanBonus), 960.0f, 430.0f, 2.4f, 2.4f, COLOR_ACCENT_GOLD, 1.2f});
                            if (EXTRA_ENABLE_RUMBLE) input_.setRumble(1.0f, 1.0f, 350);
                        }
                        const float roundTimeLimit = std::max(30.0f, 60.0f - currentRound_ * 2.5f);
                        const int timeBonus = std::max(0, static_cast<int>((roundTimeLimit - roundTime_) * 4.0f));
                        if (timeBonus > 0) {
                            totalScore_ += timeBonus;
                            roundScore_ += timeBonus;
                            floatingTexts_.push_back({"BONUS TIEMPO +" + std::to_string(timeBonus), 960.0f, 485.0f, 2.0f, 2.0f, COLOR_ACCENT_GREEN, 1.0f});
                        }
                    }
                }
            }

            // Procesar el retraso de fin de ronda y transicionar cuando expire
            if (roundEnding_) {
                roundEndTimer_ -= dt;
                if (roundEndTimer_ <= 0.0f) {
                    roundEnding_ = false;
                    audio_.playFanfare();
                    if (currentRound_ < kTotalRounds) {
                        state_ = GameState::ROUND_SUMMARY;
                    } else {
                        state_ = GameState::MATCH_OVER;
                        if (EXTRA_ENABLE_SAVED_RECORD && totalScore_ > config_.highRecord) {
                            isNewRecord_ = true;
                            config_.highRecord = totalScore_;
                            config_.save();
                            const SDL_Color colors[] = {COLOR_ACCENT_GOLD, COLOR_ACCENT_BLUE,
                                COLOR_ACCENT_RED, COLOR_ACCENT_GREEN};
                            for (int piece = 0; piece < 88; ++piece) {
                                const float spread = static_cast<float>((piece * 47) % 960) - 480.0f;
                                confetti_.push_back({960.0f + spread, -static_cast<float>((piece * 61) % 680),
                                    static_cast<float>((piece * 29) % 181) - 90.0f,
                                    110.0f + static_cast<float>((piece * 37) % 170), 5.2f,
                                    colors[piece % 4], 7 + piece % 8});
                            }
                        }
                    }
                }
            }
            break;

        case GameState::ROUND_SUMMARY:
            if (input_.confirmPressed()) {
                input_.recenterAim();
                startRound(currentRound_ + 1);
                roundBannerTimer_ = 1.8f;
                state_ = GameState::PLAYING;
            } else if (input_.cancelPressed() || input_.pausePressed()) {
                running_ = false; // Volver al menú
            }
            break;

        case GameState::MATCH_OVER:
            if (input_.confirmPressed()) {
                input_.recenterAim();
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

void ArcheryGame::shootGun() {
    audio_.playShoot();
    muzzleFlashTimer_ = 0.09f;
    shotRecoil_ = 1.0f;
    reticlePulseTimer_ = 1.0f;

    const AimState& aim = input_.getState();
    const SDL_Point muzzle = getMuzzleScreenPos();

    activeBullet_.active = true;
    activeBullet_.trail.clear();
    // La bala nace exactamente en la punta del cañón en pantalla
    activeBullet_.trail.push_back({static_cast<float>(muzzle.x), static_cast<float>(muzzle.y)});

    // Posición 3D inicial calculada para coincidir con la boca del arma proyectada
    activeBullet_.z = 1.0f;
    activeBullet_.x = (static_cast<float>(muzzle.x) - 960.0f) * activeBullet_.z / 2200.0f;
    activeBullet_.y = 1.3f - (static_cast<float>(muzzle.y) - 540.0f) * activeBullet_.z / 2200.0f;

    // Destino en el plano de las dianas coincidiendo con la mira
    float aimWorldX = aim.x * (targetDistance_ * 750.0f / 2200.0f);
    float aimWorldY = 1.3f - aim.y * (targetDistance_ * 450.0f / 2200.0f);

    float dx = aimWorldX - activeBullet_.x;
    float dy = aimWorldY - activeBullet_.y;
    float dz = targetDistance_ - activeBullet_.z;

    float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
    // Trayectoria rápida pero visible saliendo del arma (~0.095s de recorrido total)
    const float speed = std::max(180.0f, dist / 0.095f);
    activeBullet_.initialSpeed = speed;

    activeBullet_.vx = (dx / dist) * speed;
    activeBullet_.vy = (dy / dist) * speed;
    activeBullet_.vz = (dz / dist) * speed;
}

void ArcheryGame::evaluateImpact(float hitX, float hitY) {
    int hitTargetIndex = -1;
    float hitTargetX = hitX;
    float hitTargetY = hitY;
    float diffX = 0.0f;
    float diffY = 0.0f;
    float dist = std::numeric_limits<float>::max();
    for (int i = 0; i < kTargetCount; ++i) {
        if (targetFallen_[i]) continue;
        float candidateX = targetX_ + targetScreenOffsetX_[i] * targetDistance_ / 2200.0f;
        float candidateY = targetY_ - targetScreenOffsetY_[i] * targetDistance_ / 2200.0f;
        float dx = hitX - candidateX;
        float dy = hitY - candidateY;
        const int radiusPx = std::max(49, static_cast<int>((targetRadius_ / targetDistance_) * 2200.0f));
        const float bodyW = static_cast<float>(radiusPx + 28 + ((currentRound_ + i) % 3) * 8);
        const float bodyH = static_cast<float>(radiusPx * 2 + 44 + ((currentRound_ * 3 + i) % 2) * 20);
        const float halfWidthWorld = std::max(0.05f, bodyW * 0.5f * targetDistance_ / 2200.0f);
        const float halfHeightWorld = std::max(0.05f, bodyH * 0.5f * targetDistance_ / 2200.0f);
        // Colisión elíptica ajustada al cuerpo de lata que se dibuja en pantalla.
        float candidateDist = std::sqrt((dx * dx) / (halfWidthWorld * halfWidthWorld) +
                                        (dy * dy) / (halfHeightWorld * halfHeightWorld));
        if (!targetVisible_[i]) continue;
        if (candidateDist < dist) {
            hitTargetIndex = i;
            hitTargetX = candidateX;
            hitTargetY = candidateY;
            diffX = dx;
            diffY = dy;
            dist = candidateDist;
        }
    }

    SDL_Point screenHit = project3D(hitTargetX, hitTargetY, targetDistance_);

    if (hitTargetIndex >= 0 && dist <= 1.0f) {
        const bool badTarget = targetKind_[hitTargetIndex] == 1;
        if (!badTarget) {
            totalHits_++;
            ++comboHits_;
        } else {
            comboHits_ = 0;
        }

        float rFrac = dist;
        int pts = 0;
        std::string label;
        SDL_Color color;

        if (rFrac <= 0.10f) {
            pts = 100;
            label = "PREMIO MAYOR +100";
            color = COLOR_ACCENT_GOLD;
            totalBullseyes_++;
            audio_.playBullseye();
            if (EXTRA_ENABLE_RUMBLE) input_.setRumble(1.0f, 1.0f, 250);
            input_.setLED(255, 230, 80);
        } else if (rFrac <= 0.20f) {
            pts = 80;
            label = "¡PREMIO +80!";
            color = COLOR_ACCENT_GOLD;
            audio_.playBullseye();
            if (EXTRA_ENABLE_RUMBLE) input_.setRumble(0.7f, 0.9f, 200);
            input_.setLED(255, 205, 30);
        } else if (rFrac <= 0.40f) {
            pts = (rFrac <= 0.30f) ? 60 : 50;
            label = "¡ACIERTO! +" + std::to_string(pts);
            color = COLOR_ACCENT_RED;
            audio_.playImpact();
            if (EXTRA_ENABLE_RUMBLE) input_.setRumble(0.5f, 0.6f, 160);
        } else if (rFrac <= 0.60f) {
            pts = (rFrac <= 0.50f) ? 40 : 30;
            label = "¡ACIERTO! +" + std::to_string(pts);
            color = COLOR_ACCENT_BLUE;
            audio_.playImpact();
            if (EXTRA_ENABLE_RUMBLE) input_.setRumble(0.4f, 0.5f, 140);
        } else if (rFrac <= 0.80f) {
            pts = (rFrac <= 0.70f) ? 20 : 15;
            label = "¡ACIERTO! +" + std::to_string(pts);
            color = COLOR_TEXT_MUTED;
            audio_.playImpact();
            if (EXTRA_ENABLE_RUMBLE) input_.setRumble(0.3f, 0.4f, 120);
        } else {
            pts = (rFrac <= 0.90f) ? 10 : 5;
            label = "¡ACIERTO! +" + std::to_string(pts);
            color = COLOR_TEXT_MAIN;
            audio_.playImpact();
            if (EXTRA_ENABLE_RUMBLE) input_.setRumble(0.2f, 0.3f, 100);
        }

        if (badTarget) {
            pts = -50;
            label = "BLANCO MALO -50";
            color = COLOR_ACCENT_RED;
            audio_.playError();
            if (EXTRA_ENABLE_RUMBLE) input_.setRumble(0.55f, 0.7f, 180);
        } else if (targetKind_[hitTargetIndex] == 2) {
            pts *= 2;
            label = "DORADO x2 +" + std::to_string(pts);
            color = COLOR_ACCENT_GOLD;
        }

        if (!badTarget) {
            const int comboMultiplier = comboHits_ >= 4 ? 3 : (comboHits_ >= 2 ? 2 : 1);
            if (comboMultiplier > 1) {
                pts *= comboMultiplier;
                label += " COMBO x" + std::to_string(comboMultiplier);
            }
        }

        totalScore_ = std::max(0, totalScore_ + pts);
        roundScore_ += pts;
        if (pts > bestShotPoints_) bestShotPoints_ = pts;

        targetImpacts_.push_back({diffX, diffY, hitTargetIndex});
        if (!targetFallen_[hitTargetIndex]) {
            targetFallen_[hitTargetIndex] = true;
            targetBaseOffsetX_[hitTargetIndex] = targetScreenOffsetX_[hitTargetIndex];
            targetBaseOffsetY_[hitTargetIndex] = targetScreenOffsetY_[hitTargetIndex];
            targetsDownThisRound_++;
            targetFallVelocity_[hitTargetIndex] = 60.0f + (hitTargetIndex % 2) * 20.0f;
        }

        if (EXTRA_ENABLE_FLOATING_TEXTS) {
            floatingTexts_.push_back({label, static_cast<float>(screenHit.x), static_cast<float>(screenHit.y - 70), 2.2f, 2.2f, color, 1.2f});
        }
    } else {
        comboHits_ = 0;
        audio_.playError();
        if (EXTRA_ENABLE_RUMBLE) input_.setRumble(0.2f, 0.2f, 80);
        if (EXTRA_ENABLE_FLOATING_TEXTS) screenShakeTrauma_ = 0.9f;
        // Destello variable solo en fallos: algunos son rojos, otros cálidos
        // y otros no activan flash para que el efecto no se vuelva repetitivo.
        ++missFlashCount_;
        const int flashVariant = (missFlashCount_ * 7 + currentRound_) % 4;
        if (flashVariant == 0 || flashVariant == 2) {
            screenFlashColor_ = flashVariant == 0 ? SDL_Color{235, 62, 54, 255} : SDL_Color{255, 222, 123, 255};
            screenFlashAlpha_ = flashVariant == 0 ? 88 : 58;
            screenFlashTimer_ = flashVariant == 0 ? 0.095f : 0.065f;
        }
        SDL_Point missPoint = project3D(hitX, hitY, targetDistance_);
        missMarks_.push_back({static_cast<float>(missPoint.x), static_cast<float>(missPoint.y), 3.0f});
        constexpr int missPenalty = 20;
        totalScore_ = std::max(0, totalScore_ - missPenalty);
        roundScore_ -= missPenalty;
        if (EXTRA_ENABLE_FLOATING_TEXTS) {
            floatingTexts_.push_back({"FALLO -20", static_cast<float>(missPoint.x), static_cast<float>(missPoint.y - 40), 1.8f, 1.8f, COLOR_ACCENT_RED, 1.0f});
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

SDL_Point ArcheryGame::getMuzzleScreenPos() const {
    const AimState& aim = input_.getState();
    const float yaw = aim.x * 0.38f + std::sin(roundTime_ * 1.7f) * 0.010f;
    const float pitch = -0.04f + aim.y * 0.28f + std::sin(roundTime_ * 1.25f) * 0.008f;
    const float cy = std::cos(yaw), sy = std::sin(yaw);
    const float cp = std::cos(pitch), sp = std::sin(pitch);
    constexpr float focal = 1120.0f;
    constexpr float cameraY = 948.0f;
    constexpr float modelScale = 0.70f;
    struct Vertex3D { float x, y, z; };
    const Vertex3D pivot{0.0f, -0.22f, 2.35f};

    const float px = (0.0f - pivot.x) * modelScale;
    const float py = (0.21f - pivot.y) * modelScale;
    const float pz = (4.66f - pivot.z) * modelScale;
    const float rx = px * cy + pz * sy;
    const float zYaw = -px * sy + pz * cy;
    const float finalX = pivot.x + rx;
    const float finalY = pivot.y + py * cp - zYaw * sp;
    const float finalZ = pivot.z + py * sp + zYaw * cp;

    const int mx = 960 + static_cast<int>(finalX * focal / finalZ + weaponSwayX_);
    const int my = static_cast<int>(cameraY - shotRecoil_ * 26.0f - finalY * focal / finalZ);
    return {mx, my};
}

void ArcheryGame::drawText(TTF_Font* font, const std::string& text, int x, int y, SDL_Color color, bool center) {
    if (!font || text.empty()) return;
    // Sustituir símbolos que suelen faltar en las fuentes de sistema por ASCII,
    // para que nunca aparezcan cuadros vacíos en HUD ni mensajes flotantes.
    std::string safeText = text;
    const std::pair<const char*, const char*> replacements[] = {
        {"Á", "A"}, {"É", "E"}, {"Í", "I"}, {"Ó", "O"}, {"Ú", "U"},
        {"á", "a"}, {"é", "e"}, {"í", "i"}, {"ó", "o"}, {"ú", "u"},
        {"Ñ", "N"}, {"ñ", "n"}, {"¡", "!"}, {"¿", "?"},
        {"★", "*"}, {"•", "|"}, {"△", "Y"}, {"►", ">"}, {"◄", "<"}
    };
    for (const auto& replacement : replacements) {
        size_t pos = 0;
        while ((pos = safeText.find(replacement.first, pos)) != std::string::npos) {
            safeText.replace(pos, std::strlen(replacement.first), replacement.second);
            pos += std::strlen(replacement.second);
        }
    }
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, safeText.c_str(), color);
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
    if (std::abs(angle) < 0.1f) {
        SDL_Rect r = {static_cast<int>(std::lround(cx - w * 0.5f)),
                      static_cast<int>(std::lround(cy - h * 0.5f)),
                      std::max(1, static_cast<int>(std::lround(w))),
                      std::max(1, static_cast<int>(std::lround(h)))};
        SDL_RenderFillRect(ren_, &r);
        return;
    }

    // Rotar las cuatro esquinas primero y rellenar el polígono en coordenadas
    // de pantalla. Escanear filas locales producía huecos/franjas al quedar
    // la lata casi horizontal.
    const float radians = angle * 3.14159265f / 180.0f;
    const float cosA = std::cos(radians);
    const float sinA = std::sin(radians);
    const float halfW = w * 0.5f;
    const float halfH = h * 0.5f;
    SDL_FPoint points[4] = {
        {-halfW, -halfH}, {halfW, -halfH}, {halfW, halfH}, {-halfW, halfH}
    };
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    for (SDL_FPoint& p : points) {
        const float x = p.x;
        const float y = p.y;
        p.x = cx + x * cosA - y * sinA;
        p.y = cy + x * sinA + y * cosA;
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }
    const int firstY = static_cast<int>(std::floor(minY));
    const int lastY = static_cast<int>(std::ceil(maxY));
    for (int y = firstY; y <= lastY; ++y) {
        float intersections[4];
        int count = 0;
        for (int i = 0; i < 4; ++i) {
            const SDL_FPoint& a = points[i];
            const SDL_FPoint& b = points[(i + 1) % 4];
            // Sample at the scanline center to avoid one-pixel gaps between
            // adjacent rows when filling steeply rotated thin details.
            const float sampleY = static_cast<float>(y) + 0.5f;
            if ((a.y <= sampleY && b.y > sampleY) || (b.y <= sampleY && a.y > sampleY)) {
                const float t = (sampleY - a.y) / (b.y - a.y);
                intersections[count++] = a.x + t * (b.x - a.x);
            }
        }
        if (count < 2) continue;
        float leftF = intersections[0];
        float rightF = intersections[1];
        for (int i = 2; i < count; ++i) {
            leftF = std::min(leftF, intersections[i]);
            rightF = std::max(rightF, intersections[i]);
        }
        const int left = static_cast<int>(std::floor(leftF));
        const int right = static_cast<int>(std::ceil(rightF));
        SDL_RenderDrawLine(ren_, left, y, right, y);
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
    SDL_RenderSetViewport(ren_, nullptr);
    SDL_RenderSetClipRect(ren_, nullptr);
    SDL_SetRenderDrawColor(ren_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(ren_);

    // Desplazar todo el puesto brevemente cuando hay una equivocación.
    SDL_Rect sceneViewport = {shakeX, shakeY, 1920, 1080};
    SDL_RenderSetViewport(ren_, &sceneViewport);

    renderBackground();
    // Toda la geometría de blancos y soportes queda dentro del panel de juego.
    // Clip que protege el área del puesto; se extiende hasta el piso (y=852)
    // para que las latas en caída no queden recortadas durante la animación.
    SDL_Rect targetClip = {95, 405, 1730, 447};
    SDL_RenderSetClipRect(ren_, &targetClip);
    renderFairTargets();
    SDL_RenderSetClipRect(ren_, nullptr);
    renderBullet();
    renderGun();
    if (screenFlashTimer_ > 0.0f) {
        const Uint8 alpha = static_cast<Uint8>(screenFlashAlpha_ * std::clamp(screenFlashTimer_ / 0.10f, 0.0f, 1.0f));
        SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren_, screenFlashColor_.r, screenFlashColor_.g, screenFlashColor_.b, alpha);
        SDL_Rect flashOverlay = {0, 0, 1920, 1080};
        SDL_RenderFillRect(ren_, &flashOverlay);
        SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_NONE);
    }
    renderHUD();

    if (roundBannerTimer_ > 0.0f && state_ == GameState::PLAYING) {
        const float elapsed = 1.8f - roundBannerTimer_;
        float y = 174.0f;
        if (elapsed < 0.32f) {
            const float t = elapsed / 0.32f;
            y = -130.0f + (174.0f + 130.0f) * (1.0f - (1.0f - t) * (1.0f - t));
        } else if (roundBannerTimer_ < 0.42f) {
            y = 174.0f - (0.42f - roundBannerTimer_) / 0.42f * 270.0f;
        }
        drawBrutalistBox(ren_, 610, static_cast<int>(y), 700, 118, false, COLOR_CARD_BG, COLOR_ACCENT_GOLD, 10);
        drawText(fontLarge_, "RONDA " + std::to_string(currentRound_), 960, static_cast<int>(y) + 58, COLOR_TEXT_MAIN, true);
    }

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

    if (!confetti_.empty()) {
        SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
        for (const ConfettiPiece& piece : confetti_) {
            const Uint8 alpha = static_cast<Uint8>(std::clamp(piece.life / 0.8f, 0.0f, 1.0f) * 255.0f);
            SDL_SetRenderDrawColor(ren_, piece.color.r, piece.color.g, piece.color.b, alpha);
            SDL_Rect rectangle = {static_cast<int>(piece.x), static_cast<int>(piece.y), piece.size, piece.size / 2 + 2};
            SDL_RenderFillRect(ren_, &rectangle);
        }
        SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_NONE);
    }

    SDL_RenderSetViewport(ren_, nullptr);
    SDL_RenderSetClipRect(ren_, nullptr);
    // SDL_RenderSetLogicalSize puede dejar bandas laterales en ventanas más
    // anchas que 16:9. Extender el piso en esas bandas físicas conserva el
    // escenario y evita una franja clara junto al borde derecho.
    int outputW = 0, outputH = 0, logicalW = 0, logicalH = 0;
    SDL_GetRendererOutputSize(ren_, &outputW, &outputH);
    SDL_RenderGetLogicalSize(ren_, &logicalW, &logicalH);
    if (outputW > 0 && outputH > 0 && logicalW > 0 && logicalH > 0) {
        const float scale = std::min(static_cast<float>(outputW) / logicalW,
                                     static_cast<float>(outputH) / logicalH);
        const int viewportW = static_cast<int>(std::lround(logicalW * scale));
        const int viewportH = static_cast<int>(std::lround(logicalH * scale));
        const int offsetX = (outputW - viewportW) / 2;
        const int offsetY = (outputH - viewportH) / 2;
        const int floorY = offsetY + static_cast<int>(std::lround(852.0f * scale));
        const int rightStart = offsetX + viewportW;
        if (offsetX > 0 || rightStart < outputW) {
            SDL_RenderSetLogicalSize(ren_, 0, 0);
            SDL_SetRenderDrawColor(ren_, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
            SDL_Rect rightBackground = {rightStart, 0, outputW - rightStart, outputH};
            SDL_Rect leftBackground = {0, 0, offsetX, outputH};
            SDL_RenderFillRect(ren_, &rightBackground);
            SDL_RenderFillRect(ren_, &leftBackground);
            SDL_SetRenderDrawColor(ren_, 225, 229, 234, 255);
            SDL_Rect sideFloor = {rightStart, floorY, outputW - rightStart, outputH - floorY};
            SDL_Rect leftFloor = {0, floorY, offsetX, outputH - floorY};
            SDL_RenderFillRect(ren_, &sideFloor);
            SDL_RenderFillRect(ren_, &leftFloor);
            SDL_RenderSetLogicalSize(ren_, logicalW, logicalH);
        }
    }
    SDL_RenderPresent(ren_);
}

void ArcheryGame::renderBackground() {
    const SDL_Color wall = {248, 248, 244, 255};
    const SDL_Color palettes[4][3] = {
        {{35, 99, 205, 255}, {246, 205, 49, 255}, {32, 174, 164, 255}},
        {{37, 75, 150, 255}, {242, 159, 48, 255}, {221, 92, 72, 255}},
        {{75, 83, 178, 255}, {249, 199, 65, 255}, {61, 166, 151, 255}},
        {{24, 126, 164, 255}, {255, 192, 55, 255}, {211, 91, 130, 255}}
    };
    const int paletteIndex = ((currentRound_ - 1) / 3) % 4;
    const SDL_Color awningBlue = palettes[paletteIndex][0];
    const SDL_Color awningCream = palettes[paletteIndex][1];
    const SDL_Color teal = palettes[paletteIndex][2];

    const SDL_Color farBlue{static_cast<Uint8>((awningBlue.r + 255) / 2),
                            static_cast<Uint8>((awningBlue.g + 255) / 2),
                            static_cast<Uint8>((awningBlue.b + 255) / 2), 255};
    const SDL_Color farGold{static_cast<Uint8>((awningCream.r + 255) / 2),
                            static_cast<Uint8>((awningCream.g + 255) / 2),
                            static_cast<Uint8>((awningCream.b + 255) / 2), 255};
    // Puesto de feria como gran panel arquitectónico con sombras duras.
    drawBrutalistBox(ren_, 95, 160, 1730, 690, false, wall, awningBlue, 18);
    // Siluetas lejanas sobre el muro; se desplazan apenas con la mira.
    const float parallax = input_.getState().x * 14.0f;
    for (int i = 0; i < 6; ++i) {
        const int x = 115 + i * 286 + static_cast<int>(parallax * (0.35f + i * 0.08f));
        const int h = 26 + ((i * 17 + currentRound_ * 3) % 42);
        SDL_Rect farStand = {x, 272 - h, 190, h};
        const SDL_Color farColor = i % 2 == 0 ? farBlue : farGold;
        SDL_SetRenderDrawColor(ren_, farColor.r, farColor.g, farColor.b, 255);
        SDL_RenderFillRect(ren_, &farStand);
        SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
        SDL_RenderDrawRect(ren_, &farStand);
    }
    SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
    SDL_Rect counter = {95, 800, 1730, 52};
    SDL_RenderFillRect(ren_, &counter);
    SDL_SetRenderDrawColor(ren_, awningCream.r, awningCream.g, awningCream.b, 255);
    SDL_Rect counterStripe = {95, 800, 1730, 10};
    SDL_RenderFillRect(ren_, &counterStripe);
    drawText(fontSmall_, "GRAN FERIA NEO-BRUTAL  •  PUESTO DE TIRO", 960, 829, COLOR_ACCENT_GOLD, true);

    // Marquesina con banderines triangulares, en lugar de bloques sueltos.
    for (int i = 0; i < 14; ++i) {
        const int left = 112 + i * 121;
        const int center = left + 60;
        SDL_Color stripeColor = (i % 2 == 0) ? awningBlue : awningCream;
        SDL_SetRenderDrawColor(ren_, stripeColor.r, stripeColor.g, stripeColor.b, 255);
        for (int dy = 0; dy < 92; ++dy) {
            const int halfWidth = 60 * (92 - dy) / 92;
            SDL_RenderDrawLine(ren_, center - halfWidth, 285 + dy, center + halfWidth, 285 + dy);
        }
        SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
        SDL_RenderDrawLine(ren_, left, 285, center, 377);
        SDL_RenderDrawLine(ren_, left + 120, 285, center, 377);
        SDL_RenderDrawLine(ren_, left, 285, left + 120, 285);
    }
    drawBrutalistBox(ren_, 625, 175, 670, 95, true, COLOR_CARD_BG, COLOR_ACCENT_GOLD, 10);
    drawText(fontLarge_, "TIRO AL BLANCO", 960, 222, COLOR_ACCENT_GOLD, true);
    // Luces de marquesina estilo carnaval: efecto "chase" de 3 pasos con halos de glow
    const SDL_Color bulbPalette[4] = {awningCream, awningBlue, COLOR_ACCENT_GOLD, COLOR_ACCENT_RED};
    const int chaseStep = static_cast<int>(roundTime_ * 5.0f) % 3; // Cambia ~5 veces/s
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 21; ++i) {
        const int x = 130 + i * 83;
        const bool celebration = fairCelebrationTimer_ > 0.0f;
        // En celebración: parpadeo rápido alternado; en juego: chase de 3 pasos
        bool bulbOn;
        SDL_Color bulbColor;
        if (celebration) {
            bulbOn = (static_cast<int>(roundTime_ * 14.0f) + i) % 2 == 0;
            bulbColor = bulbPalette[(i + static_cast<int>(roundTime_ * 8.0f)) % 4];
        } else {
            bulbOn = (i % 3) == chaseStep;
            bulbColor = bulbPalette[i % 3];
        }
        // Halo exterior difuso cuando encendido
        if (bulbOn) {
            SDL_SetRenderDrawColor(ren_, bulbColor.r, bulbColor.g, bulbColor.b, 40);
            drawCircleFilled(x, 390, 16, {bulbColor.r, bulbColor.g, bulbColor.b, 40});
            SDL_SetRenderDrawColor(ren_, bulbColor.r, bulbColor.g, bulbColor.b, 80);
            drawCircleFilled(x, 390, 12, {bulbColor.r, bulbColor.g, bulbColor.b, 80});
        }
        // Cuerpo de la bombilla
        drawCircleFilled(x, 390, 11, COLOR_BORDER);
        drawCircleFilled(x, 390, 7, bulbOn ? bulbColor : SDL_Color{40, 40, 40, 255});
        // Reflejo blanco brillante en bombillas encendidas
        if (bulbOn) drawCircleFilled(x - 2, 387, 2, {255, 255, 255, 200});
    }
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_NONE);

    // Backboard de cinco carriles: separa el área de juego del muro y hace
    // destacar las latas sin añadir texturas ni cálculos por cuadro.
    drawBrutalistBox(ren_, 390, 405, 1140, 370, false, {190, 211, 219, 255}, COLOR_ACCENT_GOLD, 10);
    const SDL_Color laneColors[5] = {
        {207, 227, 232, 255}, {225, 233, 232, 255}, {193, 218, 225, 255},
        {225, 233, 232, 255}, {207, 227, 232, 255}
    };
    for (int lane = 0; lane < 5; ++lane) {
        SDL_Rect laneRect = {402 + lane * 223, 418, 217, 343};
        SDL_SetRenderDrawColor(ren_, laneColors[lane].r, laneColors[lane].g, laneColors[lane].b, 255);
        SDL_RenderFillRect(ren_, &laneRect);
        SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
        SDL_RenderDrawRect(ren_, &laneRect);
    }
    // Moldura de escenario; las antiguas marcas roja y azul eran decorativas,
    // no indicaban el estado de los blancos.
    SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
    SDL_Rect upperRail = {390, 405, 1140, 13};
    SDL_RenderFillRect(ren_, &upperRail);
    SDL_SetRenderDrawColor(ren_, COLOR_ACCENT_GOLD.r, COLOR_ACCENT_GOLD.g, COLOR_ACCENT_GOLD.b, 255);
    SDL_Rect railHighlight = {402, 418, 1116, 5};
    SDL_RenderFillRect(ren_, &railHighlight);
    for (const SDL_Point bolt : {SDL_Point{400, 415}, SDL_Point{1520, 415}, SDL_Point{400, 765}, SDL_Point{1520, 765}}) {
        drawCircleFilled(bolt.x, bolt.y, 8, COLOR_BORDER);
        drawCircleFilled(bolt.x, bolt.y, 3, COLOR_ACCENT_GOLD);
    }

    // Placas laterales numeradas: marco y paleta del resto de la interfaz.
    drawBrutalistBox(ren_, 145, 420, 230, 285, false, teal, awningCream, 8);
    drawText(fontMed_, "PUNTERIA", 260, 485, COLOR_CARD_BG, true);
    // Icono de blanco limpio: tres anillos concéntricos de alto contraste.
    drawCircleFilled(260, 570, 34, COLOR_BORDER);
    drawCircleFilled(260, 570, 27, COLOR_ACCENT_GOLD);
    drawCircleFilled(260, 570, 18, COLOR_CARD_BG);
    drawCircleFilled(260, 570, 9, COLOR_ACCENT_RED);
    drawCircleFilled(260, 570, 4, COLOR_CARD_BG);
    drawText(fontSmall_, "5 BLANCOS", 260, 650, COLOR_CARD_BG, true);
    drawBrutalistBox(ren_, 1545, 420, 230, 285, false, awningBlue, awningCream, 8);
    drawText(fontMed_, "GANA!", 1660, 485, COLOR_CARD_BG, true);
    for (int star = 0; star < 5; ++star) drawFlat(1578 + star * 41, 570, 20, 20, 45.0f, COLOR_ACCENT_GOLD);
    drawText(fontSmall_, "5/5 = BONUS +100", 1660, 650, COLOR_CARD_BG, true);

    // Piso de feria y líneas de perspectiva en bloque.
    SDL_SetRenderDrawColor(ren_, 225, 229, 234, 255);
    SDL_Rect floor = {0, 852, 1920, 228};
    SDL_RenderFillRect(ren_, &floor);
    SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
    for (int x = 0; x <= 1920; x += 160) SDL_RenderDrawLine(ren_, 960, 852, x, 1080);
    SDL_RenderDrawLine(ren_, 0, 930, 1920, 930);
    SDL_RenderDrawLine(ren_, 0, 1010, 1920, 1010);
    for (int tile = 0; tile < 12; ++tile) {
        SDL_Rect checker = {tile * 160, 1040, 160, 40};
        const SDL_Color tileColor = tile % 2 == 0 ? COLOR_ACCENT_BLUE : COLOR_ACCENT_GOLD;
        SDL_SetRenderDrawColor(ren_, tileColor.r, tileColor.g, tileColor.b, 255);
        SDL_RenderFillRect(ren_, &checker);
        SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
        SDL_RenderDrawRect(ren_, &checker);
    }
    return;
}

void ArcheryGame::renderFairTargets() {
    const float fov = 2200.0f;
    const SDL_Color palettes[4][5] = {
        {{231, 64, 71, 255}, {34, 132, 204, 255}, {246, 194, 41, 255}, {37, 164, 102, 255}, {143, 76, 190, 255}},
        {{130, 73, 190, 255}, {243, 120, 40, 255}, {28, 158, 158, 255}, {227, 53, 93, 255}, {40, 99, 190, 255}},
        {{44, 112, 203, 255}, {244, 76, 55, 255}, {132, 178, 54, 255}, {242, 166, 38, 255}, {155, 75, 177, 255}},
        {{245, 115, 38, 255}, {41, 164, 132, 255}, {191, 63, 129, 255}, {51, 111, 193, 255}, {230, 190, 36, 255}}
    };
    // Elipses rasterizadas por scanline: mantienen el aspecto vectorial y
    // evitan crear texturas o recursos por lata en cada cuadro.
    auto fillEllipse = [&](int cx, int cy, int rx, int ry, SDL_Color color) {
        if (rx <= 0 || ry <= 0) return;
        SDL_SetRenderDrawColor(ren_, color.r, color.g, color.b, color.a);
        for (int dy = -ry; dy <= ry; ++dy) {
            const float ratio = 1.0f - static_cast<float>(dy * dy) / static_cast<float>(ry * ry);
            const int halfWidth = static_cast<int>(rx * std::sqrt(std::max(0.0f, ratio)));
            SDL_RenderDrawLine(ren_, cx - halfWidth, cy + dy, cx + halfWidth, cy + dy);
        }
    };
    auto ellipseRim = [&](int cx, int cy, int rx, int ry, SDL_Color color) {
        fillEllipse(cx, cy, rx, ry, color);
        SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
        SDL_RenderDrawLine(ren_, cx - rx, cy, cx - rx + 2, cy - 1);
        SDL_RenderDrawLine(ren_, cx + rx - 2, cy - 1, cx + rx, cy);
        SDL_RenderDrawLine(ren_, cx - rx + 2, cy - ry, cx + rx - 2, cy - ry);
        SDL_RenderDrawLine(ren_, cx - rx + 2, cy + ry, cx + rx - 2, cy + ry);
    };
    auto fillRotatedEllipse = [&](float cx, float cy, float rx, float ry, float angle, SDL_Color color) {
        // Rasterize an ellipse in its own local coordinates, then rotate every
        // covered point. This keeps the can lids elliptical while it falls;
        // drawing them as thin rectangles turns them into long strips.
        const float radians = angle * 3.14159265f / 180.0f;
        const float cosA = std::cos(radians);
        const float sinA = std::sin(radians);
        const int localRy = std::max(1, static_cast<int>(std::ceil(ry)));
        SDL_SetRenderDrawColor(ren_, color.r, color.g, color.b, color.a);
        for (int dy = -localRy; dy <= localRy; ++dy) {
            const float ratio = 1.0f - static_cast<float>(dy * dy) / (ry * ry);
            const int halfWidth = static_cast<int>(std::ceil(rx * std::sqrt(std::max(0.0f, ratio))));
            for (int dx = -halfWidth; dx <= halfWidth; ++dx) {
                const float x = static_cast<float>(dx);
                const float y = static_cast<float>(dy);
                const int px = static_cast<int>(std::lround(cx + x * cosA - y * sinA));
                const int py = static_cast<int>(std::lround(cy + x * sinA + y * cosA));
                SDL_RenderDrawPoint(ren_, px, py);
            }
        }
    };
    for (int i = 0; i < kTargetCount; ++i) {
        const float targetWorldX = targetX_ + targetScreenOffsetX_[i] * targetDistance_ / fov;
        const float targetWorldY = targetY_ - targetScreenOffsetY_[i] * targetDistance_ / fov;
        SDL_Point center = project3D(targetWorldX, targetWorldY, targetDistance_);
        const float hitWobble = targetFallen_[i] && targetFallAngle_[i] < 14.0f
            ? std::sin(roundTime_ * 42.0f + i) * (14.0f - targetFallAngle_[i]) * 0.45f : 0.0f;
        center.x += static_cast<int>(hitWobble);
        SDL_Color canColor = palettes[(currentRound_ - 1) % 4][i];
        if (targetKind_[i] == 1) canColor = COLOR_ACCENT_RED;
        else if (targetKind_[i] == 2) canColor = COLOR_ACCENT_GOLD;
        const int radiusPx = std::max(49, static_cast<int>((targetRadius_ / targetDistance_) * fov));
        const int bodyW = radiusPx + 28 + ((currentRound_ + i) % 3) * 8;
        const int bodyH = radiusPx * 2 + 44 + ((currentRound_ * 3 + i) % 2) * 20;
        const float fall = targetFallAngle_[i];

        if (!targetVisible_[i] && !targetFallen_[i]) continue;

        // Sombra suave construida con tres elipses planas y baratas.
        SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
        fillEllipse(center.x + 8, 770, bodyW / 2 + 18, 12, {18, 22, 28, 30});
        fillEllipse(center.x + 5, 770, bodyW / 2 + 9, 8, {18, 22, 28, 34});
        fillEllipse(center.x + 3, 770, bodyW / 2, 5, {18, 22, 28, 38});
        SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_NONE);

        // Soporte grueso de feria: los cinco blancos ocupan carriles separados.
        SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
        SDL_Rect foot = {center.x - bodyW / 2 - 18, 770, bodyW + 36, 18};
        SDL_RenderFillRect(ren_, &foot);
        SDL_Rect post = {center.x - 8, center.y + bodyH / 2 - 4, 16, std::max(0, 770 - center.y - bodyH / 2 + 4)};
        if (post.h > 0) SDL_RenderFillRect(ren_, &post);

        if (targetFallen_[i]) {
            // Caída física hacia la repisa con rebote elástico.
            // Las latas de los extremos siempre caen hacia el interior del puesto para que nunca se salgan.
            const float sign = (i == 4) ? -1.0f : ((i == 0) ? 1.0f : ((i % 2 == 0) ? -1.0f : 1.0f));
            const float fallProgress = std::clamp(fall / 88.0f, 0.0f, 1.0f);
            const float angle = sign * (fallProgress * 88.0f);
            const float shelfTargetY = 770.0f - bodyW * 0.44f;
            const float bounce = std::sin(fallProgress * 3.14159f * 2.5f) * (1.0f - fallProgress) * 12.0f;
            const float fallenY = center.y + (shelfTargetY - center.y) * fallProgress + bounce;
            const float fallenX = center.x + sign * (fallProgress * bodyH * 0.38f);

            // Destello breve en el punto de impacto.
            if (fallProgress < 0.16f) {
                const Uint8 flashAlpha = static_cast<Uint8>((1.0f - fallProgress / 0.16f) * 165.0f);
                SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(ren_, 255, 239, 158, flashAlpha);
                for (int ray = 0; ray < 8; ++ray) {
                    const float rayAngle = ray * 0.785398f + i * 0.23f;
                    const int x1 = static_cast<int>(center.x + std::cos(rayAngle) * 18.0f);
                    const int y1 = static_cast<int>(center.y + std::sin(rayAngle) * 18.0f);
                    const int x2 = static_cast<int>(center.x + std::cos(rayAngle) * 44.0f);
                    const int y2 = static_cast<int>(center.y + std::sin(rayAngle) * 44.0f);
                    SDL_RenderDrawLine(ren_, x1, y1, x2, y2);
                }
                SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_NONE);
            }

            // Keep the original can body and rotate its complete silhouette.
            // Only the end caps are redrawn here; placing compact ellipses at
            // the transformed ends avoids the thin horizontal lid strips.
            drawFlat(fallenX + 7, fallenY + 7, static_cast<float>(bodyW), static_cast<float>(bodyH), angle, COLOR_BORDER);
            drawFlat(fallenX, fallenY, static_cast<float>(bodyW), static_cast<float>(bodyH), angle, canColor);
            const float innerW = static_cast<float>(bodyW - 12);
            const float innerH = static_cast<float>(bodyH - 14);
            drawFlat(fallenX - bodyW * 0.28f, fallenY, innerW * 0.12f, innerH * 0.82f,
                     angle, {255, 255, 248, 190});
            drawFlat(fallenX, fallenY, innerW * 0.70f, 10.0f, angle, COLOR_BORDER);
            drawFlat(fallenX, fallenY, innerW * 0.55f, 5.0f, angle,
                     targetKind_[i] == 2 ? COLOR_ACCENT_GOLD : COLOR_CARD_BG);

            const float capLocalY = bodyH * 0.5f - 5.0f;
            const float radians = angle * 3.14159265f / 180.0f;
            const float cosA = std::cos(radians);
            const float sinA = std::sin(radians);
            for (float localY : {-capLocalY, capLocalY}) {
                const float capX = fallenX - localY * sinA;
                const float capY = fallenY + localY * cosA;
                fillRotatedEllipse(capX, capY, bodyW * 0.46f, 6.0f, angle, COLOR_BORDER);
                fillRotatedEllipse(capX, capY, bodyW * 0.36f, 3.5f, angle, {195, 203, 208, 255});
            }
            // Chispas metálicas de impacto
            const Uint8 particleAlpha = static_cast<Uint8>((1.0f - fallProgress) * 235.0f);
            if (particleAlpha > 10) {
                SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
                for (int spark = 0; spark < 5; ++spark) {
                    const float phase = roundTime_ * (7.0f + spark) + i * 1.9f + spark * 1.25f;
                    const float spread = 18.0f + fallProgress * 76.0f;
                    const int sx = static_cast<int>(center.x + std::cos(phase) * spread);
                    const int sy = static_cast<int>(center.y - 8.0f - fallProgress * 42.0f + std::sin(phase * 1.3f) * spread * 0.45f);
                    SDL_SetRenderDrawColor(ren_, 255, 205, 66, particleAlpha);
                    SDL_RenderDrawLine(ren_, sx, sy, sx + static_cast<int>(std::cos(phase) * 9.0f),
                                       sy + static_cast<int>(std::sin(phase) * 9.0f));
                }
                SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_NONE);
            }
            continue;
        }

        SDL_Rect shadow = {center.x - bodyW / 2 + 8, center.y - bodyH / 2 + 10, bodyW, bodyH};
        SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
        SDL_RenderFillRect(ren_, &shadow);
        SDL_Rect body = {center.x - bodyW / 2, center.y - bodyH / 2, bodyW, bodyH};
        SDL_SetRenderDrawColor(ren_, canColor.r, canColor.g, canColor.b, 255);
        SDL_RenderFillRect(ren_, &body);
        SDL_Rect sideShade = {body.x + body.w - std::max(8, body.w / 7), body.y + 5,
                              std::max(5, body.w / 9), body.h - 10};
        SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 78);
        SDL_RenderFillRect(ren_, &sideShade);
        SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
        for (int b = 0; b < 4; ++b) { SDL_Rect outline = {body.x + b, body.y + b, body.w - b * 2, body.h - b * 2}; SDL_RenderDrawRect(ren_, &outline); }
        SDL_Rect shine = {body.x + body.w / 7, body.y + 13, std::max(4, body.w / 15), body.h - 26};
        SDL_SetRenderDrawColor(ren_, 255, 255, 255, 255);
        SDL_RenderFillRect(ren_, &shine);
        SDL_Rect shineEdge = {body.x + body.w / 7 + shine.w + 3, body.y + 20, 2, body.h - 40};
        SDL_SetRenderDrawColor(ren_, 255, 255, 245, 180);
        SDL_RenderFillRect(ren_, &shineEdge);
        // Reborde enrollado arriba y abajo, con tapa elíptica de metal.
        fillEllipse(center.x, body.y + 8, bodyW / 2 - 2, 11, COLOR_BORDER);
        fillEllipse(center.x, body.y + 7, bodyW / 2 - 5, 7, {210, 213, 205, 255});
        SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
        SDL_RenderDrawLine(ren_, center.x - bodyW / 2 + 7, body.y + 8, center.x + bodyW / 2 - 7, body.y + 8);
        ellipseRim(center.x, body.y + bodyH - 4, bodyW / 2 - 2, 8, {174, 183, 180, 255});

        // Panel frontal de etiqueta con filete negro: concentra la gráfica y
        // deja visibles los reflejos laterales que hacen leer el cilindro.
        SDL_Rect label = {body.x + 9, center.y - radiusPx - 4, body.w - 18, radiusPx * 2 + 8};
        SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
        SDL_RenderDrawRect(ren_, &label);

        // Cuatro diseños de lata rotativos: diana, estrella, franjas y premio.
        if (targetKind_[i] == 1) {
            drawFlat(center.x, center.y, static_cast<float>(bodyW - 22), 10.0f, 45.0f, COLOR_CARD_BG);
            drawFlat(center.x, center.y, static_cast<float>(bodyW - 22), 10.0f, -45.0f, COLOR_CARD_BG);
            drawCircleOutline(center.x, center.y, radiusPx / 2, COLOR_BORDER);
        } else if (targetKind_[i] == 2) {
            drawCircleFilled(center.x, center.y, radiusPx * 3 / 4, COLOR_ACCENT_GOLD);
            drawCircleOutline(center.x, center.y, radiusPx * 3 / 4, COLOR_BORDER);
            drawCircleFilled(center.x, center.y, radiusPx / 2, COLOR_CARD_BG);
            drawCircleFilled(center.x, center.y, radiusPx / 4, COLOR_ACCENT_GOLD);
        } else if ((currentRound_ + i) % 4 == 0) {
            drawCircleFilled(center.x, center.y, radiusPx, {250, 246, 225, 255});
            drawCircleOutline(center.x, center.y, radiusPx, COLOR_BORDER);
            drawCircleFilled(center.x, center.y, radiusPx * 2 / 3, canColor);
            drawCircleOutline(center.x, center.y, radiusPx * 2 / 3, COLOR_BORDER);
            drawCircleFilled(center.x, center.y, radiusPx / 3, COLOR_ACCENT_GOLD);
        } else if ((currentRound_ + i) % 4 == 1) {
            drawFlat(center.x, center.y, static_cast<float>(bodyW - 12), 14.0f, 0.0f, COLOR_ACCENT_GOLD);
            drawCircleFilled(center.x, center.y, radiusPx / 2, COLOR_CARD_BG);
            drawCircleOutline(center.x, center.y, radiusPx / 2, COLOR_BORDER);
        } else if ((currentRound_ + i) % 4 == 2) {
            for (int stripe = -1; stripe <= 1; ++stripe) drawFlat(center.x, center.y + stripe * 22, static_cast<float>(bodyW - 12), 9.0f, 0.0f, COLOR_CARD_BG);
            drawCircleFilled(center.x, center.y, radiusPx / 4, COLOR_ACCENT_GOLD);
        } else {
            drawCircleFilled(center.x, center.y, radiusPx * 3 / 4, COLOR_ACCENT_GOLD);
            drawCircleOutline(center.x, center.y, radiusPx * 3 / 4, COLOR_BORDER);
            drawCircleFilled(center.x, center.y, radiusPx / 2, canColor);
            drawCircleFilled(center.x, center.y, radiusPx / 5, COLOR_CARD_BG);
        }
        // Pequeño sello distintivo de feria, cambia de color por diseño.
        const SDL_Color sealColor = ((currentRound_ + i) % 2 == 0) ? COLOR_ACCENT_GOLD : COLOR_CARD_BG;
        drawFlat(center.x, center.y + radiusPx + 12, static_cast<float>(bodyW - 20), 8.0f, 0.0f, COLOR_BORDER);
        drawFlat(center.x, center.y + radiusPx + 12, static_cast<float>(bodyW - 24), 4.0f, 0.0f, sealColor);
        // Número dentro del panel frontal, lejos del borde de recorte.
        const int numberY = body.y + std::max(24, body.h / 5);
        drawBrutalistBox(ren_, center.x - 24, numberY - 14, 48, 28, false, COLOR_CARD_BG, canColor, 4);
        drawText(fontMed_, std::to_string(i + 1), center.x, numberY, COLOR_TEXT_MAIN, true);

        for (const auto& hit : targetImpacts_) {
            if (hit.targetIndex != i) continue;
            int hitX = center.x + static_cast<int>((hit.localX / targetDistance_) * fov);
            int hitY = center.y - static_cast<int>((hit.localY / targetDistance_) * fov);
            drawCircleFilled(hitX, hitY, std::max(5, radiusPx / 10), COLOR_BORDER);
            drawCircleFilled(hitX - 2, hitY - 2, std::max(2, radiusPx / 22), {255, 245, 220, 255});
        }
    }
}

void ArcheryGame::renderBullet() {
    // Marcas de error persistentes durante la ronda, dibujadas como impactos.
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
    for (const MissMark& mark : missMarks_) {
        const Uint8 alpha = static_cast<Uint8>(std::clamp(mark.life / 3.0f, 0.0f, 1.0f) * 220.0f);
        SDL_SetRenderDrawColor(ren_, COLOR_ACCENT_RED.r, COLOR_ACCENT_RED.g, COLOR_ACCENT_RED.b, alpha);
        const int x = static_cast<int>(mark.x), y = static_cast<int>(mark.y);
        SDL_RenderDrawLine(ren_, x - 10, y - 10, x + 10, y + 10);
        SDL_RenderDrawLine(ren_, x + 10, y - 10, x - 10, y + 10);
        SDL_Rect markBox = {x - 14, y - 14, 28, 28};
        SDL_RenderDrawRect(ren_, &markBox);
    }
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_NONE);
    if (!activeBullet_.active && activeBullet_.impactFlash <= 0.0f) return;

    // Trazadora luminosa de alto impacto: conecta desde la punta del cañón hasta la posición actual
    if (activeBullet_.trail.size() > 1) {
        SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
        // Capa 1: Resplandor ámbar exterior ancho
        SDL_SetRenderDrawColor(ren_, 255, 140, 20, 85);
        for (int offset = -4; offset <= 4; ++offset) {
            for (size_t i = 0; i < activeBullet_.trail.size() - 1; ++i) {
                SDL_RenderDrawLine(ren_,
                    static_cast<int>(activeBullet_.trail[i].x) + offset,
                    static_cast<int>(activeBullet_.trail[i].y),
                    static_cast<int>(activeBullet_.trail[i + 1].x) + offset,
                    static_cast<int>(activeBullet_.trail[i + 1].y));
            }
        }
        // Capa 2: Haz dorado intenso
        SDL_SetRenderDrawColor(ren_, 255, 205, 45, 180);
        for (int offset = -2; offset <= 2; ++offset) {
            for (size_t i = 0; i < activeBullet_.trail.size() - 1; ++i) {
                SDL_RenderDrawLine(ren_,
                    static_cast<int>(activeBullet_.trail[i].x) + offset,
                    static_cast<int>(activeBullet_.trail[i].y),
                    static_cast<int>(activeBullet_.trail[i + 1].x) + offset,
                    static_cast<int>(activeBullet_.trail[i + 1].y));
            }
        }
        // Capa 3: Núcleo blanco brillante
        SDL_SetRenderDrawColor(ren_, 255, 255, 235, 255);
        for (size_t i = 0; i < activeBullet_.trail.size() - 1; ++i) {
            SDL_RenderDrawLine(ren_,
                static_cast<int>(activeBullet_.trail[i].x),
                static_cast<int>(activeBullet_.trail[i].y),
                static_cast<int>(activeBullet_.trail[i + 1].x),
                static_cast<int>(activeBullet_.trail[i + 1].y));
            SDL_RenderDrawLine(ren_,
                static_cast<int>(activeBullet_.trail[i].x) + 1,
                static_cast<int>(activeBullet_.trail[i].y),
                static_cast<int>(activeBullet_.trail[i + 1].x) + 1,
                static_cast<int>(activeBullet_.trail[i + 1].y));
        }
        SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_NONE);
    }

    if (!activeBullet_.active) {
        const int radius = 25 + static_cast<int>((0.16f - activeBullet_.impactFlash) * 260.0f);
        SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
        const Uint8 alpha = static_cast<Uint8>(activeBullet_.impactFlash / 0.16f * 220.0f);
        for (int ring = 0; ring < 3; ++ring) {
            drawCircleOutline(
                static_cast<int>(activeBullet_.impactPoint.x),
                static_cast<int>(activeBullet_.impactPoint.y),
                radius + ring * 8, {255, 215, 60, alpha});
        }
        drawCircleFilled(
            static_cast<int>(activeBullet_.impactPoint.x),
            static_cast<int>(activeBullet_.impactPoint.y),
            std::max(4, static_cast<int>(18.0f * activeBullet_.impactFlash / 0.16f)),
            {255, 255, 220, alpha});
        SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_NONE);
        return;
    }

    // Proyectil en vuelo (cabeza luminosa)
    SDL_Point tip = project3D(activeBullet_.x, activeBullet_.y, activeBullet_.z);
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
    drawCircleFilled(tip.x, tip.y, 8, {255, 165, 30, 160});
    drawCircleFilled(tip.x, tip.y, 5, COLOR_ACCENT_GOLD);
    drawCircleFilled(tip.x, tip.y, 2, {255, 255, 240, 255});
    drawCircleOutline(tip.x, tip.y, 6, COLOR_BORDER);
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_NONE);
}

// =========================================================================
// Pistola del jugador, retícula y destello de disparo.
// =========================================================================
void ArcheryGame::renderGun() {
    const AimState& aim = input_.getState();

    // 1. Retícula de apuntado físico (sigue la orientación del DualShock)
    int crossX = 960 + static_cast<int>(aim.x * 750.0f);
    int crossY = HORIZON_Y + static_cast<int>(aim.y * 450.0f);

    const int ringRadius = 27 + static_cast<int>(reticlePulseTimer_ * 8.0f);
    SDL_Color reticleColor = aim.triggerHeld ? COLOR_ACCENT_RED : COLOR_BORDER;

    // Pistola low-poly en 3D por software: cada pieza es una caja con caras
    // sombreadas, rotada desde el agarre y proyectada con perspectiva.
    {
        struct Vertex { float x, y, z; };
        struct Face { std::array<Vertex, 4> vertices; SDL_Color color; float depth; };
        const float yaw = aim.x * 0.38f + std::sin(roundTime_ * 1.7f) * 0.010f;
        const float pitch = -0.04f + aim.y * 0.28f + std::sin(roundTime_ * 1.25f) * 0.008f;
        const float cy = std::cos(yaw), sy = std::sin(yaw);
        const float cp = std::cos(pitch), sp = std::sin(pitch);
        constexpr float focal = 1120.0f;
        constexpr float cameraY = 948.0f;
        const Vertex pivot{0.0f, -0.22f, 2.35f};
        std::vector<Face> faces;
        faces.reserve(128);

        auto rotate = [&](Vertex v) {
            const float px = (v.x - pivot.x) * 0.70f;
            const float py = (v.y - pivot.y) * 0.70f;
            const float pz = (v.z - pivot.z) * 0.70f;
            const float x = px * cy + pz * sy;
            const float zYaw = -px * sy + pz * cy;
            return Vertex{pivot.x + x,
                          pivot.y + py * cp - zYaw * sp,
                          pivot.z + py * sp + zYaw * cp};
        };
        auto addBox = [&](float x0, float x1, float y0, float y1, float z0, float z1,
                          SDL_Color base, SDL_Color side, SDL_Color highlight) {
            const Vertex v[8] = {
                {x0,y0,z0}, {x1,y0,z0}, {x1,y1,z0}, {x0,y1,z0},
                {x0,y0,z1}, {x1,y0,z1}, {x1,y1,z1}, {x0,y1,z1}
            };
            const int indices[6][4] = {
                {0,3,2,1}, {4,5,6,7}, {0,4,7,3},
                {1,2,6,5}, {0,1,5,4}, {3,7,6,2}
            };
            const SDL_Color shades[6] = {base, side, side, side, side, highlight};
            for (int f = 0; f < 6; ++f) {
                Vertex p[4];
                for (int i = 0; i < 4; ++i) p[i] = rotate(v[indices[f][i]]);
                const float ax = p[1].x - p[0].x, ay = p[1].y - p[0].y;
                const float bx = p[2].x - p[0].x, by = p[2].y - p[0].y;
                const float normalZ = ax * by - ay * bx;
                if (normalZ >= 0.0f) continue; // back-face culling
                float avgDepth = 0.0f;
                Face face;
                face.color = shades[f];
                for (int i = 0; i < 4; ++i) { face.vertices[i] = p[i]; avgDepth += p[i].z * 0.25f; }
                face.depth = avgDepth;
                faces.push_back(std::move(face));
            }
        };

        const SDL_Color ink{14, 16, 22, 255};
        const SDL_Color steel{194, 207, 213, 255};
        const SDL_Color steelSide{72, 91, 105, 255};
        const SDL_Color blue{34, 91, 166, 255};
        const SDL_Color blueSide{15, 43, 83, 255};
        const SDL_Color gold{245, 195, 42, 255};
        const SDL_Color glove{245, 218, 164, 255};
        const SDL_Color gloveSide{173, 137, 87, 255};

        // Mangas y manos por detrás del arma.
        addBox(-0.43f, 0.43f, -0.66f, -0.36f, 1.52f, 2.20f, blue, blueSide, {78, 145, 220, 255});
        addBox(-0.34f, 0.34f, -0.56f, -0.24f, 1.92f, 2.46f, gold, {151, 104, 10, 255}, {255, 224, 101, 255});
        addBox(-0.26f, 0.26f, -0.48f, -0.10f, 2.14f, 2.58f, glove, gloveSide, {255, 236, 190, 255});
        for (int finger = 0; finger < 3; ++finger) {
            const float y = -0.40f + finger * 0.09f;
            addBox(-0.24f, 0.24f, y, y + 0.075f, 2.47f, 2.62f,
                   glove, gloveSide, {255, 236, 190, 255});
        }

        // Empuñadura texturada, armazón, corredera y cañón con escalas de profundidad.
        addBox(-0.29f, 0.29f, -0.48f, 0.10f, 2.14f, 2.82f, blue, blueSide, {67, 137, 218, 255});
        addBox(-0.205f, 0.205f, -0.43f, 0.015f, 2.08f, 2.16f, gold, {156, 111, 12, 255}, {255, 222, 83, 255});
        addBox(-0.35f, 0.35f, 0.10f, 0.43f, 2.45f, 4.12f, steel, steelSide, {235, 242, 241, 255});
        addBox(-0.265f, 0.265f, 0.34f, 0.49f, 2.72f, 3.92f, blue, blueSide, {94, 158, 226, 255});
        addBox(-0.13f, 0.13f, 0.19f, 0.31f, 3.88f, 4.58f, ink, steelSide, {142, 158, 164, 255});
        addBox(-0.17f, 0.17f, 0.08f, 0.34f, 4.48f, 4.64f, ink, steelSide, {212, 219, 217, 255});

        // Alza y punto de mira, ventana de expulsión y guardamonte visible.
        addBox(-0.055f, 0.055f, 0.41f, 0.50f, 2.72f, 2.94f, gold, {122, 82, 8, 255}, {255, 224, 101, 255});
        addBox(-0.05f, 0.05f, 0.42f, 0.53f, 3.92f, 4.08f, gold, {122, 82, 8, 255}, {255, 224, 101, 255});
        addBox(0.345f, 0.385f, 0.20f, 0.36f, 3.02f, 3.58f, ink, {7, 22, 43, 255}, steel);
        addBox(-0.385f, -0.345f, 0.20f, 0.36f, 3.02f, 3.58f, ink, {7, 22, 43, 255}, steel);
        addBox(-0.23f, -0.19f, -0.07f, 0.02f, 2.48f, 2.71f, blue, blueSide, {67, 137, 218, 255});
        addBox(0.19f, 0.23f, -0.07f, 0.02f, 2.48f, 2.71f, blue, blueSide, {67, 137, 218, 255});
        addBox(-0.23f, 0.23f, -0.12f, -0.065f, 2.48f, 2.71f, steel, steelSide, {235, 242, 241, 255});
        addBox(-0.025f, 0.025f, 0.16f, 0.34f, 2.40f, 2.44f, gold, {151, 104, 10, 255}, {255, 224, 101, 255});
        for (int serration = 0; serration < 4; ++serration) {
            const float y = 0.15f + serration * 0.055f;
            addBox(-0.335f, -0.305f, y, y + 0.025f, 2.40f, 2.44f, ink, blueSide, steelSide);
            addBox(0.305f, 0.335f, y, y + 0.025f, 2.40f, 2.44f, ink, blueSide, steelSide);
        }
        addBox(-0.09f, 0.09f, -0.02f, 0.12f, 2.33f, 2.60f, gold, {151, 104, 10, 255}, {255, 224, 101, 255});

        // Costuras y paneles de las manos, en los mismos ejes que el arma.
        for (int ridge = 0; ridge < 4; ++ridge) {
            const float y = -0.34f + ridge * 0.075f;
            addBox(-0.21f, 0.21f, y, y + 0.018f, 2.065f, 2.085f,
                   {176, 132, 20, 255}, {102, 72, 9, 255}, {255, 223, 80, 255});
        }

        std::sort(faces.begin(), faces.end(), [](const Face& a, const Face& b) { return a.depth > b.depth; });
        for (const Face& face : faces) {
            std::array<SDL_Point, 4> points;
            for (size_t i = 0; i < face.vertices.size(); ++i) {
                const Vertex& v = face.vertices[i];
                const float safeDepth = std::max(0.5f, v.z);
                points[i] = {960 + static_cast<int>(v.x * focal / safeDepth + weaponSwayX_),
                             static_cast<int>(cameraY - v.y * focal / safeDepth)};
            }
            int minY = points.front().y, maxY = points.front().y;
            for (const SDL_Point& p : points) { minY = std::min(minY, p.y); maxY = std::max(maxY, p.y); }
            if (maxY < 0 || minY >= 1080) continue;
            minY = std::clamp(minY, 0, 1079);
            maxY = std::clamp(maxY, 0, 1079);
            SDL_SetRenderDrawColor(ren_, face.color.r, face.color.g, face.color.b, 255);
            for (int y = minY; y <= maxY; ++y) {
                std::array<int, 4> crossings{};
                size_t crossingCount = 0;
                for (size_t i = 0; i < points.size(); ++i) {
                    const SDL_Point& a = points[i];
                    const SDL_Point& b = points[(i + 1) % points.size()];
                    if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y))
                        crossings[crossingCount++] = a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y);
                }
                std::sort(crossings.begin(), crossings.begin() + crossingCount);
                for (size_t i = 0; i + 1 < crossingCount; i += 2)
                    SDL_RenderDrawLine(ren_, crossings[i], y, crossings[i + 1], y);
            }
            SDL_SetRenderDrawColor(ren_, ink.r, ink.g, ink.b, 255);
            for (size_t i = 0; i < points.size(); ++i)
                SDL_RenderDrawLine(ren_, points[i].x, points[i].y,
                                   points[(i + 1) % points.size()].x,
                                   points[(i + 1) % points.size()].y);
        }

        if (muzzleFlashTimer_ > 0.0f) {
            Vertex muzzle = rotate({0.0f, 0.21f, 4.66f});
            const int flashX = 960 + static_cast<int>(muzzle.x * focal / muzzle.z + weaponSwayX_);
            const int flashY = static_cast<int>(cameraY - muzzle.y * focal / muzzle.z);
            drawFlat(static_cast<float>(flashX), static_cast<float>(flashY), 105.0f, 58.0f, 0.0f, COLOR_ACCENT_GOLD);
            drawCircleFilled(flashX, flashY, 20, {255, 248, 210, 255});
        }

        // Dibujar la retícula al final garantiza que permanezca visible incluso
        // cuando el arma apunta justo detrás de ella.
        drawCircleOutline(crossX, crossY, ringRadius, reticleColor);
        drawCircleOutline(crossX, crossY, ringRadius - 1, reticleColor);
        drawCircleFilled(crossX, crossY, 3, reticleColor);
        SDL_SetRenderDrawColor(ren_, reticleColor.r, reticleColor.g, reticleColor.b, 255);
        SDL_RenderDrawLine(ren_, crossX - ringRadius - 8, crossY, crossX - ringRadius + 2, crossY);
        SDL_RenderDrawLine(ren_, crossX + ringRadius - 2, crossY, crossX + ringRadius + 8, crossY);
        SDL_RenderDrawLine(ren_, crossX, crossY - ringRadius - 8, crossX, crossY - ringRadius + 2);
        SDL_RenderDrawLine(ren_, crossX, crossY + ringRadius - 2, crossX, crossY + ringRadius + 14);
    }
    return;


}

// =========================================================================
// PASO 2: HUD NEO-BRUTALISTA COMPLETO
// (PUESTO, PUNTOS, RÉCORD, DISTANCIA Y MUNICIÓN)
// =========================================================================
void ArcheryGame::renderHUD() {
    SDL_Rect previousViewport;
    SDL_RenderGetViewport(ren_, &previousViewport);
    const float entryProgress = std::clamp(hudEntryTimer_ / 0.55f, 0.0f, 1.0f);
    SDL_Rect hudViewport = previousViewport;
    hudViewport.y -= static_cast<int>(entryProgress * 24.0f);
    SDL_RenderSetViewport(ren_, &hudViewport);

    if (lastHudScore_ != totalScore_) {
        if (lastHudScore_ >= 0) scorePulseTimer_ = 0.42f;
        lastHudScore_ = totalScore_;
    }
    // 1. Tarjeta Superior Izquierda: RONDA
    drawBrutalistBox(ren_, 40, 24, 260, 68, false, COLOR_CARD_BG, COLOR_ACCENT_BLUE);
    std::string roundStr = "RONDA " + std::to_string(currentRound_) + " / " + std::to_string(kTotalRounds);
    drawText(fontMed_, roundStr, 170, 58, COLOR_TEXT_MAIN, true);

    // 2. Tarjeta Superior Central: PUESTO Y DISTANCIA
    drawBrutalistBox(ren_, 740, 24, 440, 68, false, COLOR_CARD_BG, COLOR_ACCENT_GOLD);
    std::string distStr = "PUESTO " + std::to_string(currentRound_) + "  •  " + std::to_string(static_cast<int>(targetDistance_)) + " M";
    drawText(fontMed_, distStr, 960, 58, COLOR_TEXT_MAIN, true);

    // 3. Tarjeta Superior Derecha: PUNTOS Y RÉCORD
    const SDL_Color scoreAccent = scorePulseTimer_ > 0.0f ? COLOR_ACCENT_GOLD : COLOR_ACCENT_GREEN;
    drawBrutalistBox(ren_, 1440, 24, 440, 68, false, COLOR_CARD_BG, scoreAccent);
    std::string scoreStr = "PUNTOS: " + std::to_string(totalScore_);
    drawText(fontMed_, scoreStr, 1550, 58, scorePulseTimer_ > 0.0f ? COLOR_ACCENT_RED : COLOR_TEXT_MAIN, true);

    // Badge de récord
    std::string recordStr = "RÉCORD: " + std::to_string(config_.highRecord);
    drawText(fontSmall_, recordStr, 1770, 58, COLOR_ACCENT_RED, true);

    const float roundTimeLimit = std::max(30.0f, 60.0f - currentRound_ * 2.5f);
    const int remainingSeconds = std::max(0, static_cast<int>(roundTimeLimit - roundTime_));
    drawBrutalistBox(ren_, 830, 108, 260, 48, false, COLOR_CARD_BG, COLOR_ACCENT_GOLD, 4);
    drawText(fontSmall_, "BONUS TIEMPO: " + std::to_string(remainingSeconds) + " S", 960, 132, COLOR_TEXT_MAIN, true);
    if (comboHits_ >= 2) {
        const int multiplier = comboHits_ >= 4 ? 3 : 2;
        drawBrutalistBox(ren_, 1130, 108, 210, 48, false, COLOR_CARD_BG, COLOR_ACCENT_GREEN, 4);
        drawText(fontSmall_, "COMBO x" + std::to_string(multiplier), 1235, 132, COLOR_TEXT_MAIN, true);
    }

    // 4. Tarjeta Inferior Izquierda: DISPAROS RESTANTES
    drawBrutalistBox(ren_, 40, 940, 380, 100, false, COLOR_CARD_BG, COLOR_ACCENT_GOLD);
    drawText(fontSmall_, "DISPAROS RESTANTES:", 230, 962, COLOR_TEXT_MAIN, true);

    int ammoStartX = 112;
    for (int i = 0; i < kShotsPerRound; ++i) {
        int ax = ammoStartX + i * 44;
        int ay = 998;
        if (i < shotsRemaining_) {
            SDL_SetRenderDrawColor(ren_, COLOR_ACCENT_GREEN.r, COLOR_ACCENT_GREEN.g, COLOR_ACCENT_GREEN.b, 255);
            SDL_Rect round = {ax - 15, ay - 6, 30, 12};
            SDL_RenderFillRect(ren_, &round);
            SDL_SetRenderDrawColor(ren_, COLOR_ACCENT_GOLD.r, COLOR_ACCENT_GOLD.g, COLOR_ACCENT_GOLD.b, 255);
            SDL_RenderDrawLine(ren_, ax + 15, ay, ax + 21, ay);
        } else {
            drawFlat(static_cast<float>(ax), static_cast<float>(ay), 30.0f, 12.0f, 0.0f, {195, 195, 190, 255});
            SDL_SetRenderDrawColor(ren_, COLOR_BORDER.r, COLOR_BORDER.g, COLOR_BORDER.b, 255);
            SDL_Rect emptyRound = {ax - 15, ay - 6, 30, 12};
            SDL_RenderDrawRect(ren_, &emptyRound);
        }
    }
    drawText(fontSmall_, "START / ESC: AJUSTES", 230, 1017, COLOR_TEXT_MUTED, true);
    drawText(fontSmall_, "C: CENTRAR MIRA", 230, 1038, COLOR_TEXT_MUTED, true);

    // 5. Tarjeta Inferior Derecha: ESTADO DEL ARMA
    const AimState& aim = input_.getState();
    drawBrutalistBox(ren_, 1500, 940, 380, 100, false, COLOR_CARD_BG, COLOR_ACCENT_RED);
    drawText(fontSmall_, aim.triggerHeld ? "DISPARO" : "APUNTA Y DISPARA", 1690, 965,
             aim.triggerHeld ? COLOR_ACCENT_RED : COLOR_TEXT_MAIN, true);
    drawText(fontMed_, "R2  /  CLIC IZQUIERDO", 1690, 1008, COLOR_TEXT_MUTED, true);
    SDL_RenderSetViewport(ren_, &previousViewport);
}

// =========================================================================
// PASO 3: MENÚ DE AJUSTES (START/ESC)
// Opciones: Continuar, Centrar mira, Reiniciar, Volver al menú, Salir
// =========================================================================
void ArcheryGame::renderPauseMenu() {
    // Oscurecimiento de fondo
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren_, 15, 15, 18, 180);
    SDL_Rect fullScreen = { 0, 0, 1920, 1080 };
    SDL_RenderFillRect(ren_, &fullScreen);
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_NONE);

    int cardW = 740;
    int cardH = 570;
    int cardX = 960 - cardW / 2;
    int cardY = 540 - cardH / 2;

    drawBrutalistBox(ren_, cardX, cardY, cardW, cardH, false, COLOR_CARD_BG, COLOR_ACCENT_GOLD, 16);

    drawText(fontLarge_, "MENÚ DE AJUSTES", 960, cardY + 55, COLOR_TEXT_MAIN, true);
    drawText(fontSmall_, "USA [ARRIBA / ABAJO] Y [X / ENTER]", 960, cardY + 105, COLOR_TEXT_MUTED, true);

    std::vector<std::string> options = {
        "CONTINUAR",
        "SENSIBILIDAD: " + std::to_string(static_cast<int>(config_.sensitivityX * 100.0f + 0.5f)) + "%",
        "ZONA MUERTA: " + std::to_string(static_cast<int>(config_.deadzone * 100.0f + 0.5f)) + "%",
        "CENTRAR MIRA AQUI",
        "REINICIAR DESDE RONDA 1",
        "VOLVER AL MENÚ",
        "SALIR DEL JUEGO"
    };

    int startY = cardY + 140;
    for (size_t i = 0; i < options.size(); ++i) {
        bool selected = (static_cast<int>(i) == pauseMenuIndex_);
        int optY = startY + static_cast<int>(i * 50);

        if (selected) {
            // Tarjeta seleccionada con borde grueso y acento dorado
            int tagW = 660;
            int tagH = 48;
            int tagX = 960 - tagW / 2;
            drawBrutalistBox(ren_, tagX, optY - tagH / 2, tagW, tagH, false, COLOR_CARD_MUTED, COLOR_ACCENT_GOLD, 6);
            std::string selText = ">  " + options[i] + "  <";
            drawText(fontMed_, selText, 960, optY, COLOR_TEXT_MAIN, true);
        } else {
            drawText(fontMed_, options[i], 960, optY, COLOR_TEXT_MUTED, true);
        }
    }

    drawText(fontSmall_, "IZQ / DER: AJUSTAR    O / ESC: VOLVER", 960, cardY + cardH - 28, COLOR_TEXT_MUTED, true);
}

void ArcheryGame::renderCalibrationScreen() {
    int cardW = 1000;
    int cardH = 580;
    int cardX = 960 - cardW / 2;
    int cardY = 540 - cardH / 2;

    drawBrutalistBox(ren_, cardX, cardY, cardW, cardH, false, COLOR_CARD_BG, COLOR_ACCENT_GOLD, 16);

    drawText(fontLarge_, "CALIBRAR MIRA DE TIRO", 960, cardY + 60, COLOR_TEXT_MAIN, true);

    drawText(fontMed_, "Sostén el mando en una posición cómoda para ti.", 960, cardY + 160, COLOR_TEXT_MAIN, true);
    drawText(fontSmall_, "Al confirmar, esa postura será el centro de la mira.", 960, cardY + 210, COLOR_TEXT_MUTED, true);

    // Cuadro de controles
    int boxW = 820;
    int boxH = 180;
    int boxX = 960 - boxW / 2;
    int boxY = cardY + 260;
    drawBrutalistBox(ren_, boxX, boxY, boxW, boxH, false, COLOR_CARD_MUTED, {0,0,0,0}, 6);

    drawText(fontSmall_, "Orientacion DualShock: apunta a las latas", 960, boxY + 35, COLOR_TEXT_MAIN, true);
    drawText(fontSmall_, "Pulsa R2 o clic izquierdo para disparar", 960, boxY + 75, COLOR_TEXT_MAIN, true);
    drawText(fontSmall_, "Cinco disparos por ronda. Apunta al centro", 960, boxY + 115, COLOR_TEXT_MAIN, true);
    drawText(fontSmall_, "Tecla C: centrar desde tu postura actual", 960, boxY + 150, COLOR_ACCENT_RED, true);

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

    std::string rPts = std::string("Puntos en esta ronda: ") + (roundScore_ > 0 ? "+" : "") + std::to_string(roundScore_);
    drawText(fontMed_, rPts, 960, cardY + 160, roundScore_ < 0 ? COLOR_ACCENT_RED : COLOR_ACCENT_GREEN, true);

    std::string totalPts = "Puntuación acumulada: " + std::to_string(totalScore_) + " PUNTOS";
    drawText(fontMed_, totalPts, 960, cardY + 225, COLOR_TEXT_MAIN, true);

    std::string nextDist = "Próximo puesto: nivel " + std::to_string(currentRound_ + 1);
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

    int totalShots = kTotalRounds * kShotsPerRound;
    float accuracy = totalShots > 0 ? (static_cast<float>(totalHits_) / totalShots) * 100.0f : 0.0f;
    std::string accStr = "Precisión: " + std::to_string(static_cast<int>(accuracy)) + "% (" +
                         std::to_string(totalHits_) + " / " + std::to_string(totalShots) + " objetivos acertados)";
    drawText(fontMed_, accStr, 960, cardY + 295, COLOR_TEXT_MAIN, true);

    std::string bullseyeStr = "Premios mayores: " + std::to_string(totalBullseyes_);
    drawText(fontMed_, bullseyeStr, 960, cardY + 360, COLOR_ACCENT_GOLD, true);

    std::string bestShotStr = "Mejor disparo: +" + std::to_string(bestShotPoints_) + " PTS";
    drawText(fontMed_, bestShotStr, 960, cardY + 420, COLOR_TEXT_MAIN, true);

    drawText(fontMed_, "Presiona [X / ENTER] para Jugar de Nuevo", 960, cardY + 520, COLOR_ACCENT_GREEN, true);
    drawText(fontSmall_, "Presiona [O / ESC] para Salir al Menú Principal", 960, cardY + 575, COLOR_TEXT_MUTED, true);
}
