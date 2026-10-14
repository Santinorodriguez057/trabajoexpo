#include "input.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

InputManager::InputManager(GameConfig& config)
    : config_(config) {
}

InputManager::~InputManager() {
    closeController();
}

bool InputManager::init() {
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");

    openFirstController();
    calibrate();
    return true;
}

void InputManager::openFirstController() {
    closeController();
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            controller_ = SDL_GameControllerOpen(i);
            if (controller_) {
                hasGyro_ = SDL_GameControllerHasSensor(controller_, SDL_SENSOR_GYRO);
                hasAccel_ = SDL_GameControllerHasSensor(controller_, SDL_SENSOR_ACCEL);

                if (hasGyro_) SDL_GameControllerSetSensorEnabled(controller_, SDL_SENSOR_GYRO, SDL_TRUE);
                if (hasAccel_) SDL_GameControllerSetSensorEnabled(controller_, SDL_SENSOR_ACCEL, SDL_TRUE);

                // LED inicial: Verde azulado calmado (listo)
                setLED(20, 180, 200);
                break;
            }
        }
    }
}

void InputManager::closeController() {
    if (controller_) {
        setLED(0, 0, 0);
        SDL_GameControllerClose(controller_);
        controller_ = nullptr;
        hasGyro_ = false;
        hasAccel_ = false;
    }
}

void InputManager::calibrate() {
    targetAimX_ = 0.0f;
    targetAimY_ = 0.0f;
    state_.x = 0.0f;
    state_.y = 0.0f;
    state_.calibrated = true;

    if (controller_ && hasGyro_) {
        float g[3] = {0, 0, 0};
        SDL_GameControllerGetSensorData(controller_, SDL_SENSOR_GYRO, g, 3);
        gyroBiasX_ = g[0];
        gyroBiasY_ = g[1];
        gyroBiasZ_ = g[2];
    }

    // Feedback táctil suave de confirmación de centrado
    setRumble(0.2f, 0.4f, 150);
}

float InputManager::applyDeadzone(float value, float deadzone) {
    float mag = std::abs(value);
    if (mag < deadzone) return 0.0f;
    float sign = (value > 0.0f) ? 1.0f : -1.0f;
    return sign * (mag - deadzone) / (1.0f - deadzone);
}

void InputManager::setRumble(float lowFreq, float highFreq, Uint32 durationMs) {
    if (!config_.rumbleEnabled || !controller_) return;
    Uint16 low = static_cast<Uint16>(std::clamp(lowFreq, 0.0f, 1.0f) * 65535.0f);
    Uint16 high = static_cast<Uint16>(std::clamp(highFreq, 0.0f, 1.0f) * 65535.0f);
    SDL_GameControllerRumble(controller_, low, high, durationMs);
}

void InputManager::setLED(Uint8 r, Uint8 g, Uint8 b) {
    if (controller_) {
        SDL_GameControllerSetLED(controller_, r, g, b);
    }
}

void InputManager::clearActionFlags() {
    pausePressed_ = false;
    confirmPressed_ = false;
    cancelPressed_ = false;
    state_.justFired = false;
}

void InputManager::handleEvent(const SDL_Event& ev) {
    if (ev.type == SDL_CONTROLLERDEVICEADDED) {
        if (!controller_) openFirstController();
    } else if (ev.type == SDL_CONTROLLERDEVICEREMOVED) {
        if (controller_ && ev.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller_))) {
            closeController();
        }
    } else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
        if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_START || ev.cbutton.button == SDL_CONTROLLER_BUTTON_BACK) {
            pausePressed_ = true;
        } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_Y || ev.cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
            // Triangulo o L1 recalibra / recentra la mira
            calibrate();
        } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
            confirmPressed_ = true;
        } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
            cancelPressed_ = true;
        }
    } else if (ev.type == SDL_KEYDOWN) {
        if (ev.key.keysym.sym == SDLK_ESCAPE) {
            pausePressed_ = true;
        } else if (ev.key.keysym.sym == SDLK_c || ev.key.keysym.sym == SDLK_r) {
            calibrate();
        } else if (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_SPACE) {
            confirmPressed_ = true;
        }
    } else if (ev.type == SDL_MOUSEMOTION) {
        // Fallback de apuntado con ratón si no se usa mando
        if (!controller_ || !hasGyro_) {
            targetAimX_ += (ev.motion.xrel * 0.0025f) * config_.sensitivityX;
            float yChange = (ev.motion.yrel * 0.0025f) * config_.sensitivityY;
            if (config_.invertY) yChange = -yChange;
            targetAimY_ += yChange;
            targetAimX_ = std::clamp(targetAimX_, -1.1f, 1.1f);
            targetAimY_ = std::clamp(targetAimY_, -0.9f, 0.9f);
        }
    } else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
        mouseHeld_ = true;
    } else if (ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_LEFT) {
        mouseHeld_ = false;
    }
}

void InputManager::update(float dt) {
    if (dt <= 0.0f) dt = 0.016f;

    // 1. Apuntado con sensores de movimiento del DualShock
    if (controller_ && hasGyro_) {
        float gyro[3] = {0, 0, 0};
        SDL_GameControllerGetSensorData(controller_, SDL_SENSOR_GYRO, gyro, 3);

        // Descontar sesgo de reposo
        float gx = gyro[0] - gyroBiasX_;
        float gy = gyro[1] - gyroBiasY_;
        float gz = gyro[2] - gyroBiasZ_;

        // Giroscopio DualShock 4:
        // gy: giro sobre eje vertical (yaw / mirar izq-der)
        // gz: inclinación lateral tipo volante (roll)
        // gx: cabeceo arriba/abajo (pitch)
        // Combinamos yaw y roll para que apuntar tanto girando muñeca como inclinando se sienta 100% natural
        float turnRate = -gy - 0.70f * gz;
        float pitchRate = -gx;

        if (config_.invertY) pitchRate = -pitchRate;

        float filteredTurn = applyDeadzone(turnRate, config_.deadzone);
        float filteredPitch = applyDeadzone(pitchRate, config_.deadzone);

        // Integración de orientación angular a coordenadas de mira
        float moveX = filteredTurn * config_.sensitivityX * dt * 1.6f;
        float moveY = filteredPitch * config_.sensitivityY * dt * 1.6f;

        targetAimX_ += moveX;
        targetAimY_ += moveY;

        // Limitar dentro del campo visual útil
        targetAimX_ = std::clamp(targetAimX_, -1.15f, 1.15f);
        targetAimY_ = std::clamp(targetAimY_, -0.90f, 0.90f);

        // Stick derecho como ajuste auxiliar
        Sint16 rx = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_RIGHTX);
        Sint16 ry = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_RIGHTY);
        if (std::abs(rx) > 4000) {
            targetAimX_ += (rx / 32767.0f) * config_.sensitivityX * dt * 1.2f;
            targetAimX_ = std::clamp(targetAimX_, -1.15f, 1.15f);
        }
        if (std::abs(ry) > 4000) {
            float stickY = (ry / 32767.0f);
            if (config_.invertY) stickY = -stickY;
            targetAimY_ += stickY * config_.sensitivityY * dt * 1.2f;
            targetAimY_ = std::clamp(targetAimY_, -0.90f, 0.90f);
        }
    }

    // 2. Filtro de suavizado (Exponential Smoothing)
    float smoothFactor = 1.0f - std::pow(std::clamp(config_.smoothing, 0.0f, 0.95f), dt * 60.0f);
    state_.x += (targetAimX_ - state_.x) * smoothFactor;
    state_.y += (targetAimY_ - state_.y) * smoothFactor;

    // 3. Procesamiento del Gatillo R2 (Progresivo)
    float triggerVal = 0.0f;
    if (controller_) {
        Sint16 rawTrigger = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        if (rawTrigger > 1000) {
            triggerVal = static_cast<float>(rawTrigger) / 32767.0f;
        }
    }

    // Fallback con ratón / teclado
    if (mouseHeld_) {
        mouseTension_ = std::min(1.0f, mouseTension_ + dt * 1.8f);
        triggerVal = std::max(triggerVal, mouseTension_);
    } else {
        mouseTension_ = 0.0f;
    }

    bool wasDrawing = state_.isDrawing;

    if (triggerVal > 0.08f) {
        state_.isDrawing = true;
        // La tensión aumenta progresivamente hacia el nivel de presión del gatillo
        if (currentTension_ < triggerVal) {
            currentTension_ = std::min(triggerVal, currentTension_ + dt * 2.2f);
        } else {
            currentTension_ = triggerVal;
        }
        state_.tension = std::clamp(currentTension_, 0.0f, 1.0f);

        // Feedback háptico mientras se tensa el arco
        float rumbleIntensity = state_.tension * 0.35f;
        setRumble(rumbleIntensity * 0.5f, rumbleIntensity, 50);

        // LED cambia progresivamente de amarillo a naranja brillante y carmesí
        Uint8 ledR = static_cast<Uint8>(std::min(255.0f, 60.0f + state_.tension * 195.0f));
        Uint8 ledG = static_cast<Uint8>(std::max(0.0f, 200.0f - state_.tension * 160.0f));
        Uint8 ledB = 10;
        setLED(ledR, ledG, ledB);

    } else {
        // Gatillo liberado
        if (wasDrawing) {
            if (currentTension_ >= 0.15f) {
                // ¡Disparo efectuado!
                state_.justFired = true;
                state_.releaseTension = currentTension_;
                // Golpe seco de retroceso en vibración
                setRumble(0.8f, 1.0f, 120);
                // Destello dorado/blanco momentáneo
                setLED(255, 230, 100);
            } else {
                // Canceló el tensado sin suficiente fuerza
                setLED(20, 180, 200);
            }
        } else {
            state_.justFired = false;
            setLED(20, 180, 200);
        }
        state_.isDrawing = false;
        currentTension_ = 0.0f;
        state_.tension = 0.0f;
    }
}
