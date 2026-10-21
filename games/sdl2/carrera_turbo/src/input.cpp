#include "input.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

InputManager::InputManager(DrivingConfig& config)
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

                // LED inicial: Cian neón
                setLED(0, 220, 255);
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
    if (controller_ && hasAccel_) {
        float accel[3] = {0, 0, -9.8f};
        SDL_GameControllerGetSensorData(controller_, SDL_SENSOR_ACCEL, accel, 3);
        calibratedRoll_ = std::atan2(accel[0], -accel[2]);
        currentRoll_ = calibratedRoll_;
    } else {
        calibratedRoll_ = 0.0f;
        currentRoll_ = 0.0f;
    }
    targetSteering_ = 0.0f;
    state_.steering = 0.0f;
    state_.calibrated = true;
    setRumble(0.2f, 0.4f, 150);
}

float InputManager::applyDeadzone(float val, float dz) {
    float mag = std::abs(val);
    if (mag < dz) return 0.0f;
    float sign = (val > 0.0f) ? 1.0f : -1.0f;
    return sign * (mag - dz) / (1.0f - dz);
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

void InputManager::clearMenuFlags() {
    state_.confirm = false;
    state_.cancel = false;
    state_.pause = false;
}

void InputManager::handleEvent(const SDL_Event& ev) {
    if (ev.type == SDL_CONTROLLERDEVICEADDED) {
        if (!controller_) openFirstController();
    } else if (ev.type == SDL_CONTROLLERDEVICEREMOVED) {
        if (controller_ && ev.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller_))) {
            closeController();
        }
    } else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
        if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) state_.confirm = true;
        else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_B) state_.cancel = true;
        else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_START || ev.cbutton.button == SDL_CONTROLLER_BUTTON_BACK) state_.pause = true;
        else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_Y || ev.cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
            // Triángulo o L1 recalibra el volante centrado
            calibrate();
        }
    } else if (ev.type == SDL_KEYDOWN) {
        if (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_SPACE) state_.confirm = true;
        if (ev.key.keysym.sym == SDLK_ESCAPE) { state_.cancel = true; state_.pause = true; }
        if (ev.key.keysym.sym == SDLK_c || ev.key.keysym.sym == SDLK_r) calibrate();

        if (ev.key.keysym.sym == SDLK_LEFT || ev.key.keysym.sym == SDLK_a) keyLeft_ = true;
        if (ev.key.keysym.sym == SDLK_RIGHT || ev.key.keysym.sym == SDLK_d) keyRight_ = true;
        if (ev.key.keysym.sym == SDLK_UP || ev.key.keysym.sym == SDLK_w) keyUp_ = true;
        if (ev.key.keysym.sym == SDLK_DOWN || ev.key.keysym.sym == SDLK_s) keyDown_ = true;
        if (ev.key.keysym.sym == SDLK_SPACE) keySpace_ = true;
    } else if (ev.type == SDL_KEYUP) {
        if (ev.key.keysym.sym == SDLK_LEFT || ev.key.keysym.sym == SDLK_a) keyLeft_ = false;
        if (ev.key.keysym.sym == SDLK_RIGHT || ev.key.keysym.sym == SDLK_d) keyRight_ = false;
        if (ev.key.keysym.sym == SDLK_UP || ev.key.keysym.sym == SDLK_w) keyUp_ = false;
        if (ev.key.keysym.sym == SDLK_DOWN || ev.key.keysym.sym == SDLK_s) keyDown_ = false;
        if (ev.key.keysym.sym == SDLK_SPACE) keySpace_ = false;
    }
}

void InputManager::update(float dt) {
    if (dt <= 0.0f) dt = 0.016f;

    float steerInput = 0.0f;
    float throttleInput = 0.0f;
    float brakeInput = 0.0f;
    bool handbrakeInput = keySpace_;

    // 1. Control con Giroscopio / Acelerómetro del DualShock (Modo Volante Físico)
    if (controller_ && hasGyro_ && hasAccel_) {
        float gyro[3] = {0, 0, 0};
        float accel[3] = {0, 0, -9.8f};
        SDL_GameControllerGetSensorData(controller_, SDL_SENSOR_GYRO, gyro, 3);
        SDL_GameControllerGetSensorData(controller_, SDL_SENSOR_ACCEL, accel, 3);

        // Ángulo de inclinación lateral (volante estilo Gran Turismo):
        // Girar el volante a la derecha inclina la mano derecha hacia abajo (accel[0] negativo).
        // Girar el volante a la izquierda inclina la mano izquierda hacia abajo (accel[0] positivo).
        float rollFromAccel = std::atan2(accel[0], -accel[2]);
        // Fusión complementaria con el giroscopio para 0 latencia y 0 temblor
        currentRoll_ = 0.92f * (currentRoll_ + gyro[1] * dt) + 0.08f * rollFromAccel;

        float deltaRoll = currentRoll_ - calibratedRoll_;

        // Inclinación a grados físicos
        state_.physicalAngleDeg = deltaRoll * 57.2957795f;

        // Zona muerta
        float filteredRoll = applyDeadzone(deltaRoll, config_.deadzone);

        // ~35° (0.61 rad) representa giro completo de dirección
        const float kMaxWheelAngle = 0.60f;
        steerInput = -(filteredRoll / kMaxWheelAngle) * config_.steeringSensitivity;

        // Gatillos progresivos analógicos
        Sint16 rawR2 = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        Sint16 rawL2 = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
        if (rawR2 > 600) throttleInput = static_cast<float>(rawR2) / 32767.0f;
        if (rawL2 > 600) brakeInput = static_cast<float>(rawL2) / 32767.0f;

        // Botón Cuadrado o X para freno de mano / derrape
        if (SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_X)) {
            handbrakeInput = true;
        }

        // Stick izquierdo como ajuste o fallback
        Sint16 stickX = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_LEFTX);
        if (std::abs(stickX) > 6000) {
            steerInput += (stickX / 32767.0f) * config_.steeringSensitivity;
        }
    }

    // 2. Fallback de teclado
    if (keyLeft_) steerInput -= 1.0f;
    if (keyRight_) steerInput += 1.0f;
    if (keyUp_) throttleInput = std::max(throttleInput, 1.0f);
    if (keyDown_) brakeInput = std::max(brakeInput, 1.0f);

    steerInput = std::clamp(steerInput, -1.0f, 1.0f);

    // Suavizado del volante
    float smoothAlpha = 1.0f - std::pow(std::clamp(config_.steeringSmoothing, 0.0f, 0.95f), dt * 60.0f);
    state_.steering += (steerInput - state_.steering) * smoothAlpha;
    state_.throttle = std::clamp(throttleInput, 0.0f, 1.0f);
    state_.brake = std::clamp(brakeInput, 0.0f, 1.0f);
    state_.handbrake = handbrakeInput;
}
