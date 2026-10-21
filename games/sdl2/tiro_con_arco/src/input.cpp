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
    // Si tenemos sensores, tomar la posición actual del mando como el centro físico (0,0)
    if (controller_ && hasAccel_) {
        float accel[3] = {0, 0, -9.8f};
        SDL_GameControllerGetSensorData(controller_, SDL_SENSOR_ACCEL, accel, 3);

        calibratedPitch_ = std::atan2(accel[1], -accel[2]);
        calibratedRoll_ = std::atan2(accel[0], -accel[2]);
        currentPitchAngle_ = calibratedPitch_;
        currentRollAngle_ = calibratedRoll_;
    } else {
        calibratedPitch_ = 0.20f;
        calibratedRoll_ = 0.0f;
        currentPitchAngle_ = 0.20f;
        currentRollAngle_ = 0.0f;
    }

    currentYawAngle_ = 0.0f;
    targetAimX_ = 0.0f;
    targetAimY_ = 0.0f;
    state_.x = 0.0f;
    state_.y = 0.0f;
    state_.calibrated = true;

    // Vibración suave de confirmación
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
            // Triángulo o L1 recalibra / recentra la mira instantáneamente
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
        // Fallback de ratón si no hay mando con giroscopio
        if (!controller_ || !hasGyro_) {
            targetAimX_ += (ev.motion.xrel * 0.003f) * config_.sensitivityX;
            float yChange = (ev.motion.yrel * 0.003f) * config_.sensitivityY;
            if (config_.invertY) yChange = -yChange;
            targetAimY_ += yChange;
            targetAimX_ = std::clamp(targetAimX_, -1.15f, 1.15f);
            targetAimY_ = std::clamp(targetAimY_, -0.90f, 0.90f);
        }
    } else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
        mouseHeld_ = true;
    } else if (ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_LEFT) {
        mouseHeld_ = false;
    }
}

void InputManager::update(float dt) {
    if (dt <= 0.0f) dt = 0.016f;

    // 1. Apuntado por ORIENTACIÓN FÍSICA con sensores DualShock 4
    if (controller_ && hasGyro_ && hasAccel_) {
        float gyro[3] = {0, 0, 0};
        float accel[3] = {0, 0, -9.8f};
        SDL_GameControllerGetSensorData(controller_, SDL_SENSOR_GYRO, gyro, 3);
        SDL_GameControllerGetSensorData(controller_, SDL_SENSOR_ACCEL, accel, 3);

        // Vector de gravedad normalizado
        float norm = std::sqrt(accel[0]*accel[0] + accel[1]*accel[1] + accel[2]*accel[2]);
        float gx_norm = 0.0f, gy_norm = 0.2f, gz_norm = -0.98f;
        if (norm > 0.01f) {
            gx_norm = accel[0] / norm;
            gy_norm = accel[1] / norm;
            gz_norm = accel[2] / norm;
        }

        // A. PITCH (Inclinación vertical absoluta calculada desde el acelerómetro)
        // En posición neutra: accel.y ≈ +2.04, accel.z ≈ -9.91
        // Inclinar arriba: accel.y sube -> pitch aumenta
        // Inclinar abajo: accel.y baja -> pitch disminuye
        float pitchFromAccel = std::atan2(accel[1], -accel[2]);
        // Filtro complementario: respuesta inmediata del giro sin vibración
        currentPitchAngle_ = 0.93f * (currentPitchAngle_ + (-gyro[0]) * dt) + 0.07f * pitchFromAccel;

        // B. ROLL (Inclinación lateral tipo volante desde el acelerómetro)
        float rollFromAccel = std::atan2(accel[0], -accel[2]);
        currentRollAngle_ = 0.93f * (currentRollAngle_ + gyro[1] * dt) + 0.07f * rollFromAccel;

        // C. YAW (Giro horizontal izquierda / derecha alrededor del eje de gravedad)
        // Rotación alrededor de la gravedad vertical (combina gyro[2] y gyro[1] según el ángulo)
        float yawRate = (gyro[0] * gx_norm + gyro[1] * gy_norm + gyro[2] * gz_norm);
        float filteredYawRate = applyDeadzone(yawRate, config_.deadzone);
        currentYawAngle_ += filteredYawRate * dt;

        // Combinación de giro de muñeca (Yaw) + inclinación de volante (Roll)
        float deltaPitch = currentPitchAngle_ - calibratedPitch_;
        float deltaRoll = currentRollAngle_ - calibratedRoll_;
        float effectiveYaw = currentYawAngle_ - 0.55f * deltaRoll;

        // Mapeo angular corregido según orientación física del DualShock:
        // - Apuntar arriba -> mueve arriba
        // - Apuntar abajo -> mueve abajo
        // - Mover derecha -> mueve derecha
        // - Mover izquierda -> mueve izquierda
        const float kScaleX = 1.0f / 0.26f;
        const float kScaleY = 1.0f / 0.20f;

        float horizComponent = (deltaPitch * kScaleX) * config_.sensitivityX;
        float vertComponent = -(effectiveYaw * kScaleY) * config_.sensitivityY;

        targetAimX_ = horizComponent;
        if (config_.invertY) vertComponent = -vertComponent;
        targetAimY_ = vertComponent;

        // Ajuste auxiliar con stick analógico derecho si se desea recortar
        Sint16 rx = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_RIGHTX);
        Sint16 ry = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_RIGHTY);
        if (std::abs(rx) > 5000) {
            calibratedPitch_ -= (rx / 32767.0f) * dt * 0.8f;
        }
        if (std::abs(ry) > 5000) {
            float stickY = (ry / 32767.0f) * dt * 0.8f;
            if (config_.invertY) stickY = -stickY;
            currentYawAngle_ += stickY;
        }

        // Límites útiles del campo de tiro
        targetAimX_ = std::clamp(targetAimX_, -1.15f, 1.15f);
        targetAimY_ = std::clamp(targetAimY_, -0.90f, 0.90f);
    }

    // 2. Filtro de suavizado (Exponential Smoothing) para máxima fluidez
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

    // Fallback con ratón
    if (mouseHeld_) {
        mouseTension_ = std::min(1.0f, mouseTension_ + dt * 1.8f);
        triggerVal = std::max(triggerVal, mouseTension_);
    } else {
        mouseTension_ = 0.0f;
    }

    bool wasDrawing = state_.isDrawing;

    if (triggerVal > 0.08f) {
        state_.isDrawing = true;
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
        if (wasDrawing) {
            if (currentTension_ >= 0.15f) {
                state_.justFired = true;
                state_.releaseTension = currentTension_;
                setRumble(0.8f, 1.0f, 120);
                setLED(255, 230, 100);
            } else {
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
