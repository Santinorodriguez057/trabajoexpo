#include "input.h"
#include <cmath>
#include <algorithm>

TouchInput::TouchInput(int playerIndex, SDL_Color color)
    : playerIndex_(playerIndex), color_(color) {}

TouchInput::~TouchInput() {
    if (controller_) {
        setLED(0, 0, 0);
    }
}

void TouchInput::setController(SDL_GameController* gc) {
    controller_ = gc;
    if (controller_) {
        setLED(color_.r, color_.g, color_.b);
    }
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
            trailPoints_.push_back({currentX_, currentY_, 1.0f, 18.0f});
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
            trailPoints_.push_back({currentX_, currentY_, 1.0f, 18.0f});
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
                    trailPoints_.push_back({sx, sy, 1.0f, 18.0f});
                } else {
                    float dist = std::hypot(sx - currentX_, sy - currentY_);
                    if (dist > 3.0f) {
                        currentX_ = sx;
                        currentY_ = sy;
                        touchHistory_.push_back({sx, sy, 0.0f});
                        trailPoints_.push_back({sx, sy, 1.0f, 18.0f});
                    }
                }
            } else {
                fingerDown_ = false;
            }
        }
#endif
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
            activeSlashes_.push_back({pOld.x, pOld.y, pNew.x, pNew.y, angle, speed, playerIndex_});
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
