#include "input.h"
#include <cmath>
#include <algorithm>

TouchInput::TouchInput(int playerIndex, SDL_Color color)
    : playerIndex_(playerIndex), color_(color) {}

TouchInput::~TouchInput() {
    if (controller_) {
        setLED(0, 0, 0);
        SDL_GameControllerClose(controller_);
        controller_ = nullptr;
    }
    if (arcadeStick_) {
        SDL_JoystickClose(arcadeStick_);
        arcadeStick_ = nullptr;
    }
}

void TouchInput::setController(SDL_GameController* gc) {
    if (controller_ && controller_ != gc) {
        setLED(0, 0, 0);
        SDL_GameControllerClose(controller_);
    }
    controller_ = gc;
    if (controller_) {
        setLED(color_.r, color_.g, color_.b);
    }
}

void TouchInput::setArcadeStick(SDL_Joystick* joy) {
    if (arcadeStick_ && arcadeStick_ != joy) {
        SDL_JoystickClose(arcadeStick_);
    }
    arcadeStick_ = joy;
}

void TouchInput::triggerHaptic(float low, float high, Uint32 ms) {
    if (!controller_) return;
    Uint16 l = static_cast<Uint16>(std::clamp(low, 0.0f, 1.0f) * 65535.0f);
    Uint16 h = static_cast<Uint16>(std::clamp(high, 0.0f, 1.0f) * 65535.0f);
    SDL_GameControllerRumble(controller_, l, h, ms);
}

void TouchInput::setLED(Uint8 r, Uint8 g, Uint8 b) {
    if (controller_) {
        SDL_GameControllerSetLED(controller_, r, g, b);
    }
}

void TouchInput::handleEvent(const SDL_Event& ev, int screenW, int screenH) {
    if (!controller_) return;

    SDL_JoystickID myId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller_));

    if (ev.type == SDL_CONTROLLERTOUCHPADDOWN) {
        if (ev.ctouchpad.which == myId && ev.ctouchpad.touchpad == 0 && ev.ctouchpad.finger == 0) {
            fingerDown_ = true;
            currentX_ = ev.ctouchpad.x * screenW;
            currentY_ = ev.ctouchpad.y * screenH;
            prevX_ = currentX_;
            prevY_ = currentY_;
            touchHistory_.clear();
            touchHistory_.push_back({currentX_, currentY_, 0.0f});
            float tWidth = dualBlade_ ? 48.0f : 26.0f;
            trailPoints_.push_back({currentX_, currentY_, 1.0f, tWidth});
        }
    } else if (ev.type == SDL_CONTROLLERTOUCHPADMOTION) {
        if (ev.ctouchpad.which == myId && ev.ctouchpad.touchpad == 0 && ev.ctouchpad.finger == 0) {
            if (!fingerDown_) {
                fingerDown_ = true;
                prevX_ = ev.ctouchpad.x * screenW;
                prevY_ = ev.ctouchpad.y * screenH;
            }
            currentX_ = ev.ctouchpad.x * screenW;
            currentY_ = ev.ctouchpad.y * screenH;
            touchHistory_.push_back({currentX_, currentY_, 0.0f});
            float tWidth = dualBlade_ ? 48.0f : 26.0f;
            trailPoints_.push_back({currentX_, currentY_, 1.0f, tWidth});
        }
    } else if (ev.type == SDL_CONTROLLERTOUCHPADUP) {
        if (ev.ctouchpad.which == myId && ev.ctouchpad.touchpad == 0 && ev.ctouchpad.finger == 0) {
            fingerDown_ = false;
        }
    }
}

void TouchInput::update(float dt, int screenW, int screenH) {
    activeSlashes_.clear();

    // 1. Polling directo como respaldo de la API de SDL si el event loop no reporta ráfagas completas
    if (controller_) {
#if SDL_VERSION_ATLEAST(2, 0, 14)
        Uint8 state = 0;
        float fx = 0.0f, fy = 0.0f, pressure = 0.0f;
        if (SDL_GameControllerGetTouchpadFinger(controller_, 0, 0, &state, &fx, &fy, &pressure) == 0) {
            if (state == SDL_PRESSED) {
                float sx = fx * screenW;
                float sy = fy * screenH;

                if (!fingerDown_) {
                    fingerDown_ = true;
                    prevX_ = sx;
                    prevY_ = sy;
                    currentX_ = sx;
                    currentY_ = sy;
                    touchHistory_.clear();
                    touchHistory_.push_back({sx, sy, 0.0f});
                    trailPoints_.push_back({sx, sy, 1.0f, 26.0f});
                } else {
                    float dist = std::hypot(sx - currentX_, sy - currentY_);
                    if (dist > 3.0f) {
                        currentX_ = sx;
                        currentY_ = sy;
                        touchHistory_.push_back({sx, sy, 0.0f});
                        float tWidth = dualBlade_ ? 48.0f : 26.0f;
                        trailPoints_.push_back({sx, sy, 1.0f, tWidth});
                    }
                }
            } else {
                fingerDown_ = false;
            }
        }
#endif
    }

    // 1.2 Soporte de Fight Stick / Palanca Arcade como katana
    if (arcadeStick_) {
        // Ejes X/Y del stick o Hat switch
        Sint16 stickX = SDL_JoystickGetAxis(arcadeStick_, 0);
        Sint16 stickY = SDL_JoystickGetAxis(arcadeStick_, 1);

        float dirX = 0.0f;
        float dirY = 0.0f;
        if (std::abs(stickX) > 8000) dirX = stickX / 32767.0f;
        if (std::abs(stickY) > 8000) dirY = stickY / 32767.0f;

        // Comprobar D-pad si los ejes están en 0
        if (std::abs(dirX) < 0.1f && std::abs(dirY) < 0.1f && SDL_JoystickNumHats(arcadeStick_) > 0) {
            Uint8 hat = SDL_JoystickGetHat(arcadeStick_, 0);
            if (hat & SDL_HAT_LEFT) dirX = -1.0f;
            if (hat & SDL_HAT_RIGHT) dirX = 1.0f;
            if (hat & SDL_HAT_UP) dirY = -1.0f;
            if (hat & SDL_HAT_DOWN) dirY = 1.0f;
        }

        bool isStickMoved = (std::abs(dirX) > 0.2f || std::abs(dirY) > 0.2f);
        bool btnSlash = false;
        int nBtns = SDL_JoystickNumButtons(arcadeStick_);
        for (int b = 0; b < nBtns && b < 8; ++b) {
            if (SDL_JoystickGetButton(arcadeStick_, b)) {
                btnSlash = true;
                break;
            }
        }

        if (isStickMoved || btnSlash) {
            float moveSpeed = 1600.0f; // Velocidad de corte rápido
            arcadeCursorX_ = std::clamp(arcadeCursorX_ + dirX * moveSpeed * dt, 40.0f, static_cast<float>(screenW - 40));
            arcadeCursorY_ = std::clamp(arcadeCursorY_ + dirY * moveSpeed * dt, 40.0f, static_cast<float>(screenH - 40));

            currentX_ = arcadeCursorX_;
            currentY_ = arcadeCursorY_;
            fingerDown_ = true;

            touchHistory_.push_back({currentX_, currentY_, 0.0f});
            float tWidth = dualBlade_ ? 48.0f : 26.0f;
            trailPoints_.push_back({currentX_, currentY_, 1.0f, tWidth});
        } else {
            if (fingerDown_ && !controller_) {
                fingerDown_ = false;
            }
        }
    }

    // 2. Procesar puntos táctiles para construir segmentos de espada válidos
    // Aumentar el tiempo en la historia
    for (auto& p : touchHistory_) {
        p.time += dt;
    }
    // Mantener solo los últimos ~0.15 segundos para cortes instantáneos y receptivos
    touchHistory_.erase(
        std::remove_if(touchHistory_.begin(), touchHistory_.end(),
                       [](const TouchPoint& p) { return p.time > 0.16f; }),
        touchHistory_.end());

    if (touchHistory_.size() >= 2) {
        const auto& pOld = touchHistory_.front();
        const auto& pNew = touchHistory_.back();

        float dx = pNew.x - pOld.x;
        float dy = pNew.y - pOld.y;
        float dist = std::hypot(dx, dy);
        float timeDiff = std::max(0.001f, pNew.time - pOld.time);

        // Umbral de corte mínimo: distancia >= 24px y velocidad rápida
        if (dist >= 24.0f) {
            float speed = dist / timeDiff;
            float angle = std::atan2(dy, dx);
            float thick = dualBlade_ ? 42.0f : 14.0f;
            activeSlashes_.push_back({pOld.x, pOld.y, pNew.x, pNew.y, angle, speed, playerIndex_, thick});
        }
    }

    // 3. Envejecer y desvanecer la estela neobrutalista de la katana
    for (auto& tp : trailPoints_) {
        tp.life -= dt * 6.5f; // Desvanecimiento rápido y nítido
        tp.width = std::max(0.0f, tp.width - dt * 45.0f);
    }
    trailPoints_.erase(
        std::remove_if(trailPoints_.begin(), trailPoints_.end(),
                       [](const SlashTrailPoint& tp) { return tp.life <= 0.0f; }),
        trailPoints_.end());
}

