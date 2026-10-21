#include "input.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

InputManager::InputManager(GameConfig& config)
    : config_(config) {
    // Inicializar con el sesgo físico medido del mando
    gyroBiasX_ = 0.007580f;
    gyroBiasY_ = -0.012994f;
    gyroBiasZ_ = -0.002166f;
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

                // LED inicial: Azul turquesa listo
                setLED(20, 180, 220);
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
    drawAnchorAimX_ = 0.0f;
    drawAnchorAimY_ = 0.0f;
    postShotFreezeTimer_ = 0.0f;
    state_.calibrated = true;

    // Valores de referencia del giroscopio del mando en reposo
    const float kBenchBiasX = 0.007580f;
    const float kBenchBiasY = -0.012994f;
    const float kBenchBiasZ = -0.002166f;

    if (controller_ && hasGyro_) {
        float sumX = 0.0f, sumY = 0.0f, sumZ = 0.0f;
        float minX = 999.0f, maxX = -999.0f;
        float minY = 999.0f, maxY = -999.0f;
        float minZ = 999.0f, maxZ = -999.0f;
        const int kSamples = 25;
        for (int i = 0; i < kSamples; ++i) {
            float g[3] = {0, 0, 0};
            SDL_GameControllerGetSensorData(controller_, SDL_SENSOR_GYRO, g, 3);
            sumX += g[0]; sumY += g[1]; sumZ += g[2];
            minX = std::min(minX, g[0]); maxX = std::max(maxX, g[0]);
            minY = std::min(minY, g[1]); maxY = std::max(maxY, g[1]);
            minZ = std::min(minZ, g[2]); maxZ = std::max(maxZ, g[2]);
            SDL_Delay(2);
        }
        // Solo actualizar el sesgo de hardware si el mando está perfectamente inmóvil sobre una mesa
        if ((maxX - minX < 0.007f) && (maxY - minY < 0.007f) && (maxZ - minZ < 0.007f)) {
            gyroBiasX_ = sumX / kSamples;
            gyroBiasY_ = sumY / kSamples;
            gyroBiasZ_ = sumZ / kSamples;
        } else {
            gyroBiasX_ = kBenchBiasX;
            gyroBiasY_ = kBenchBiasY;
            gyroBiasZ_ = kBenchBiasZ;
        }
    } else {
        gyroBiasX_ = kBenchBiasX;
        gyroBiasY_ = kBenchBiasY;
        gyroBiasZ_ = kBenchBiasZ;
    }

    // Feedback táctil de recentrado
    setRumble(0.20f, 0.40f, 140);
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
        if (!controller_ || !hasGyro_) {
            targetAimX_ += (ev.motion.xrel * 0.0028f) * config_.sensitivityX;
            float yChange = (ev.motion.yrel * 0.0028f) * config_.sensitivityY;
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
    if (dt > 0.05f) dt = 0.05f;

    // Temporizador de congelamiento para absorber el rebote mecánico del gatillo al soltar
    if (postShotFreezeTimer_ > 0.0f) {
        postShotFreezeTimer_ = std::max(0.0f, postShotFreezeTimer_ - dt);
    }

    // 1. Procesamiento previo del gatillo R2
    float triggerVal = 0.0f;
    if (controller_) {
        Sint16 rawTrigger = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        if (rawTrigger > 1000) {
            triggerVal = static_cast<float>(rawTrigger) / 32767.0f;
        }
    }

    if (mouseHeld_) {
        mouseTension_ = std::min(1.0f, mouseTension_ + dt * 2.0f);
        triggerVal = std::max(triggerVal, mouseTension_);
    } else {
        mouseTension_ = 0.0f;
    }

    bool wasDrawing = state_.isDrawing;

    if (triggerVal > 0.08f) {
        if (!wasDrawing) {
            // Anclar punto de mira de inicio de tensado
            drawAnchorAimX_ = targetAimX_;
            drawAnchorAimY_ = targetAimY_;
        }
        state_.isDrawing = true;
        if (currentTension_ < triggerVal) {
            currentTension_ = std::min(triggerVal, currentTension_ + dt * 3.0f);
        } else {
            currentTension_ = triggerVal;
        }
        state_.tension = std::clamp(currentTension_, 0.0f, 1.0f);

        // LED progresivo de tensión (Oro -> Ámbar -> Carmesí)
        Uint8 ledR = static_cast<Uint8>(std::min(255.0f, 60.0f + state_.tension * 195.0f));
        Uint8 ledG = static_cast<Uint8>(std::max(0.0f, 200.0f - state_.tension * 160.0f));
        Uint8 ledB = 10;
        setLED(ledR, ledG, ledB);

    } else {
        if (wasDrawing) {
            if (currentTension_ >= 0.15f) {
                state_.justFired = true;
                state_.releaseTension = currentTension_;
                // Congelar la integración giroscópica 180ms para ignorar el coletazo del dedo al soltar
                postShotFreezeTimer_ = 0.18f;
                setLED(255, 230, 100);
            } else {
                setLED(20, 180, 220);
            }
        } else {
            state_.justFired = false;
        }
        state_.isDrawing = false;
        currentTension_ = 0.0f;
        state_.tension = 0.0f;
    }

    // 2. Apuntado con sensores del DualShock 4
    if (controller_ && hasGyro_) {
        float gyro[3] = {0, 0, 0};
        SDL_GameControllerGetSensorData(controller_, SDL_SENSOR_GYRO, gyro, 3);

        // Descontar sesgo de reposo exacto
        float gx = gyro[0] - gyroBiasX_;
        float gy = gyro[1] - gyroBiasY_;
        float gz = gyro[2] - gyroBiasZ_;

        // Si estamos en la ventana de absorción post-disparo, no integrar movimiento espurio
        if (postShotFreezeTimer_ > 0.0f) {
            gx = 0.0f;
            gy = 0.0f;
            gz = 0.0f;
        }

        // DualShock 4:
        // Al tensar con el dedo índice en R2, la mano aplica un par de torsión natural (roll/gz).
        // Al tensar el arco se desacopla gz por completo para que presionar R2 no tire la mira a un costado.
        float rollWeight = state_.isDrawing ? 0.0f : 0.25f;
        float turnRate = -gy - rollWeight * gz;
        float pitchRate = -gx;

        if (config_.invertY) pitchRate = -pitchRate;

        // Amortiguación de apuntado al tensar (Steady Aim / Modo Foco):
        // Al tensar mucho el arco, la mira se estabiliza fuertemente (hasta un 86% de reducción a 100% tensión)
        // permitiendo afinar el tiro con micro-movimientos sin que la mira tiemble o se dispare.
        float tensionDamping = 1.0f / (1.0f + state_.tension * state_.tension * 6.0f);
        float effectiveDeadzone = config_.deadzone * (1.0f + state_.tension * 2.8f);

        float filteredTurn = applyDeadzone(turnRate, effectiveDeadzone);
        float filteredPitch = applyDeadzone(pitchRate, effectiveDeadzone);

        // Integración de velocidad angular con amortiguación
        float moveX = filteredTurn * config_.sensitivityX * dt * 2.0f * tensionDamping;
        float moveY = filteredPitch * config_.sensitivityY * dt * 2.0f * tensionDamping;

        targetAimX_ += moveX;
        targetAimY_ += moveY;

        // Si se está tensando el arco, delimitar la deriva alrededor del punto de inicio del tensado
        // Esto garantiza que por más que se apriete fuerte R2, la mira no puede irse fuera del rango de diana
        if (state_.isDrawing) {
            float maxDevX = 0.15f;
            float maxDevY = 0.11f;
            targetAimX_ = std::clamp(targetAimX_, drawAnchorAimX_ - maxDevX, drawAnchorAimX_ + maxDevX);
            targetAimY_ = std::clamp(targetAimY_, drawAnchorAimY_ - maxDevY, drawAnchorAimY_ + maxDevY);
        }

        // Limitar dentro del campo visual
        targetAimX_ = std::clamp(targetAimX_, -1.15f, 1.15f);
        targetAimY_ = std::clamp(targetAimY_, -0.90f, 0.90f);

        // Stick derecho como ajuste analógico auxiliar suave
        Sint16 rx = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_RIGHTX);
        Sint16 ry = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_RIGHTY);
        if (std::abs(rx) > 3500) {
            targetAimX_ += (rx / 32767.0f) * config_.sensitivityX * dt * 1.5f * tensionDamping;
            targetAimX_ = std::clamp(targetAimX_, -1.15f, 1.15f);
        }
        if (std::abs(ry) > 3500) {
            float stickY = (ry / 32767.0f);
            if (config_.invertY) stickY = -stickY;
            targetAimY_ += stickY * config_.sensitivityY * dt * 1.5f * tensionDamping;
            targetAimY_ = std::clamp(targetAimY_, -0.90f, 0.90f);
        }
    }

    // 3. Filtro de suavizado (Smooth Interpolation)
    float smoothFactor = 1.0f - std::pow(std::clamp(config_.smoothing, 0.0f, 0.90f), dt * 60.0f);
    state_.x += (targetAimX_ - state_.x) * smoothFactor;
    state_.y += (targetAimY_ - state_.y) * smoothFactor;
