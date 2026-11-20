#include "input.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

InputManager::InputManager(GameConfig& config)
    : config_(config) {}

InputManager::~InputManager() {
    closeController();
}

bool InputManager::init() {
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");

    openFirstController();
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
                accelFilterInitialized_ = false;

                // LED inicial: Azul turquesa listo
                setLED(20, 180, 220);
                break;
            }
        }
    }
}

void InputManager::closeController() {
    gyroSpeedSamples_.clear();
    gyroSpeedSampleSeconds_ = 0.0f;
    if (controller_) {
        setLED(0, 0, 0);
        SDL_GameControllerClose(controller_);
        controller_ = nullptr;
        hasGyro_ = false;
        hasAccel_ = false;
    }
}

void InputManager::calibrate() {
    completeCalibration();
}

void InputManager::beginCalibration() {
    calibrationGyroSum_[0] = calibrationGyroSum_[1] = calibrationGyroSum_[2] = 0.0;
    calibrationGyroSamples_ = 0;
    telemetry_.postureValid = false;
}

void InputManager::accumulateCalibrationSample() {
    if (!controller_ || !hasGyro_ || !telemetry_.postureValid) return;
    for (int axis = 0; axis < 3; ++axis) calibrationGyroSum_[axis] += telemetry_.rawGyro[axis];
    ++calibrationGyroSamples_;
}

void InputManager::completeCalibration() {
    if (controller_ && hasGyro_) {
        if (calibrationGyroSamples_ > 0) {
            gyroBiasX_ = static_cast<float>(calibrationGyroSum_[0] / calibrationGyroSamples_);
            gyroBiasY_ = static_cast<float>(calibrationGyroSum_[1] / calibrationGyroSamples_);
            gyroBiasZ_ = static_cast<float>(calibrationGyroSum_[2] / calibrationGyroSamples_);
        } else {
            gyroBiasX_ = telemetry_.rawGyro[0];
            gyroBiasY_ = telemetry_.rawGyro[1];
            gyroBiasZ_ = telemetry_.rawGyro[2];
        }
    }
    targetAimX_ = 0.0f;
    targetAimY_ = 0.0f;
    state_.x = 0.0f;
    state_.y = 0.0f;
    calibratedPitch_ = telemetry_.pitchDegrees * 0.0174532925f;
    calibratedRoll_ = telemetry_.rollDegrees * 0.0174532925f;
    calibratedYaw_ = 0.0f; // El acelerómetro no mide rumbo; el sesgo del gyro fija el cero.
    for (int axis = 0; axis < 3; ++axis) calibratedAccel_[axis] = telemetry_.accel[axis];
    currentPitchAngle_ = calibratedPitch_;
    currentYawAngle_ = calibratedYaw_;
    currentRollAngle_ = calibratedRoll_;
    postShotFreezeTimer_ = 0.0f;
    filteredTurnRate_ = 0.0f;
    filteredPitchRate_ = 0.0f;
    state_.calibrated = true;
    beginCalibration();
    setRumble(0.20f, 0.40f, 140);
}

void InputManager::recenterAim() {
    targetAimX_ = 0.0f;
    targetAimY_ = 0.0f;
    state_.x = 0.0f;
    state_.y = 0.0f;
    filteredTurnRate_ = 0.0f;
    filteredPitchRate_ = 0.0f;
    postShotFreezeTimer_ = 0.12f;
    rumbleSuppressTimer_ = std::max(rumbleSuppressTimer_, 0.12f);
    state_.calibrated = true;
}

bool InputManager::takeRecenterRequest() {
    const bool requested = recenterRequested_;
    recenterRequested_ = false;
    return requested;
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
    if (SDL_GameControllerRumble(controller_, low, high, durationMs) == 0) {
        // El motor transmite vibración al giroscopio; ignorar ese rebote en la mira.
        rumbleSuppressTimer_ = std::max(rumbleSuppressTimer_, durationMs / 1000.0f + 0.06f);
    }
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
        } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
            confirmPressed_ = true;
        } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
            cancelPressed_ = true;
        }
    } else if (ev.type == SDL_KEYDOWN) {
        if (ev.key.keysym.sym == SDLK_ESCAPE) {
            pausePressed_ = true;
        } else if (ev.key.keysym.sym == SDLK_c || ev.key.keysym.sym == SDLK_r) {
            recenterKeyDown_ = true;
        } else if (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_SPACE) {
            confirmPressed_ = true;
        }
    } else if (ev.type == SDL_KEYUP && (ev.key.keysym.sym == SDLK_c || ev.key.keysym.sym == SDLK_r)) {
        recenterKeyDown_ = false;
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
    dt = std::clamp(dt, 0.0f, 0.05f);

    telemetry_.hasGyro = controller_ && hasGyro_;
    telemetry_.hasAccel = controller_ && hasAccel_;
    telemetry_.gyroSpeed = 0.0f;
    bool gyroReadSucceeded = false;
    if (controller_ && hasGyro_) {
        float gyro[3] = {};
        if (SDL_GameControllerGetSensorData(controller_, SDL_SENSOR_GYRO, gyro, 3) == 0) {
            gyroReadSucceeded = true;
            for (int axis = 0; axis < 3; ++axis) telemetry_.rawGyro[axis] = gyro[axis];
            const float gx = gyro[0] - gyroBiasX_;
            const float gy = gyro[1] - gyroBiasY_;
            const float gz = gyro[2] - gyroBiasZ_;
            telemetry_.gyroSpeed = std::sqrt(gx * gx + gy * gy + gz * gz);
        }
    }
    if (dt > 0.0f) {
        const float instantaneousSpeed = gyroReadSucceeded
            ? telemetry_.gyroSpeed : CalibrationTuning::kQuietGyroRadPerSec * 2.0f;
        gyroSpeedSamples_.push_back({dt, instantaneousSpeed});
        gyroSpeedSampleSeconds_ += dt;
        while (gyroSpeedSampleSeconds_ > CalibrationTuning::kGyroAverageWindowSeconds &&
               !gyroSpeedSamples_.empty()) {
            const float excess = gyroSpeedSampleSeconds_ - CalibrationTuning::kGyroAverageWindowSeconds;
            if (gyroSpeedSamples_.front().first <= excess) {
                gyroSpeedSampleSeconds_ -= gyroSpeedSamples_.front().first;
                gyroSpeedSamples_.pop_front();
            } else {
                gyroSpeedSamples_.front().first -= excess;
                gyroSpeedSampleSeconds_ -= excess;
            }
        }
        if (!gyroSpeedSamples_.empty() && gyroSpeedSampleSeconds_ > 0.0f) {
            float weightedSpeed = 0.0f;
            for (const auto& sample : gyroSpeedSamples_) weightedSpeed += sample.first * sample.second;
            telemetry_.gyroSpeed = weightedSpeed / gyroSpeedSampleSeconds_;
        }
    }
    telemetry_.quietness = std::clamp(
        1.0f - telemetry_.gyroSpeed / CalibrationTuning::kQuietnessScoreRangeRadPerSec, 0.0f, 1.0f);
    if (controller_ && hasAccel_) {
        float accel[3] = {};
        if (SDL_GameControllerGetSensorData(controller_, SDL_SENSOR_ACCEL, accel, 3) == 0) {
            for (int axis = 0; axis < 3; ++axis) telemetry_.rawAccel[axis] = accel[axis];
            if (!accelFilterInitialized_) {
                for (int axis = 0; axis < 3; ++axis) filteredAccel_[axis] = accel[axis];
                accelFilterInitialized_ = true;
            } else {
                const float alpha = dt > 0.0f
                    ? 1.0f - std::exp(-dt / CalibrationTuning::kAccelLowPassSeconds) : 0.0f;
                for (int axis = 0; axis < 3; ++axis)
                    filteredAccel_[axis] += (accel[axis] - filteredAccel_[axis]) * alpha;
            }
            for (int axis = 0; axis < 3; ++axis) telemetry_.accel[axis] = filteredAccel_[axis];

            // SDL2 defines controller axes as X=left/right, Y=bottom/top,
            // Z=far/near. At rest, gravity is positive on Y, not Z.
            const float ax = filteredAccel_[0];
            const float ay = filteredAccel_[1];
            const float az = filteredAccel_[2];
            telemetry_.rollDegrees = std::atan2(ax, ay) * 57.2957795f;
            telemetry_.pitchDegrees = std::atan2(az, std::sqrt(ax * ax + ay * ay)) * 57.2957795f;
        }
    }
    telemetry_.postureValid = telemetry_.hasGyro &&
        telemetry_.gyroSpeed < CalibrationTuning::kQuietGyroRadPerSec &&
        telemetry_.quietness >= CalibrationTuning::kMinimumQuietness;

    const bool controllerRecenterHeld = controller_ &&
        (SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_Y) ||
         SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_LEFTSHOULDER));
    const bool recenterHeld = controllerRecenterHeld || recenterKeyDown_;
    if (recenterHeld && !recenterLatched_) {
        recenterHoldProgress_ = std::min(CalibrationTuning::kRecenterHoldSeconds, recenterHoldProgress_ + dt);
        if (recenterHoldProgress_ >= CalibrationTuning::kRecenterHoldSeconds) {
            recenterRequested_ = true;
            recenterHoldProgress_ = 0.0f;
            recenterLatched_ = true;
        }
    } else {
        if (!recenterHeld) {
            recenterHoldProgress_ = 0.0f;
            recenterLatched_ = false;
        }
    }

    rumbleSuppressTimer_ = std::max(0.0f, rumbleSuppressTimer_ - dt);

    // Temporizador de congelamiento para absorber el rebote mecánico del gatillo al soltar
    if (postShotFreezeTimer_ > 0.0f) {
        postShotFreezeTimer_ = std::max(0.0f, postShotFreezeTimer_ - dt);
    }

    // Disparo instantáneo: el flanco de presión del R2 o del clic consume un tiro.
    float triggerVal = 0.0f;
    if (controller_) {
        Sint16 rawTrigger = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        if (rawTrigger > 1000) {
            triggerVal = static_cast<float>(rawTrigger) / 32767.0f;
        }
    }

    if (mouseHeld_) triggerVal = 1.0f;

    bool wasDrawing = state_.triggerHeld;
    bool triggerDown = triggerVal > 0.18f;
    state_.triggerHeld = triggerDown;
    state_.justFired = triggerDown && !wasDrawing;
    if (state_.justFired) {
        postShotFreezeTimer_ = 0.10f;
        setLED(255, 210, 60);
    } else if (!triggerDown && wasDrawing) {
        setLED(20, 180, 220);
    }

    // 2. Apuntado con sensores del DualShock 4
    if (controller_ && hasGyro_) {
        float gyro[3] = {telemetry_.rawGyro[0], telemetry_.rawGyro[1], telemetry_.rawGyro[2]};

        // Descontar sesgo de reposo exacto
        float gx = gyro[0] - gyroBiasX_;
        float gy = gyro[1] - gyroBiasY_;
        float gz = gyro[2] - gyroBiasZ_;

        // Aprendizaje lento del cero cuando el mando casi no se mueve evita
        // que un pequeño sesgo del giroscopio haga derivar la mira con el tiempo.
        if (!state_.triggerHeld && postShotFreezeTimer_ <= 0.0f && rumbleSuppressTimer_ <= 0.0f &&
            std::abs(gx) < 0.055f && std::abs(gy) < 0.055f && std::abs(gz) < 0.055f) {
            const float biasAlpha = std::min(1.0f, dt * 0.12f);
            gyroBiasX_ += gx * biasAlpha;
            gyroBiasY_ += gy * biasAlpha;
            gyroBiasZ_ += gz * biasAlpha;
            gx = gyro[0] - gyroBiasX_;
            gy = gyro[1] - gyroBiasY_;
            gz = gyro[2] - gyroBiasZ_;
        }

        // Si estamos en la ventana de absorción post-disparo, no integrar movimiento espurio
        if (postShotFreezeTimer_ > 0.0f || rumbleSuppressTimer_ > 0.0f) {
            gx = 0.0f;
            gy = 0.0f;
            gz = 0.0f;
        }

        // DualShock 4:
        // Al apretar R2 la mano aplica torsión; durante el disparo reducimos el eje de roll.
        float rollWeight = state_.triggerHeld ? 0.0f : 0.25f;
        float turnRate = -gy - rollWeight * gz;
        float pitchRate = -gx;

        // Filtro pasa-bajos: atenúa la vibración mecánica del mando sin perder el apuntado.
        const float filterTime = state_.triggerHeld ? 0.14f : 0.055f;
        const float filterAlpha = dt > 0.0f ? dt / (filterTime + dt) : 0.0f;
        filteredTurnRate_ += (turnRate - filteredTurnRate_) * filterAlpha;
        filteredPitchRate_ += (pitchRate - filteredPitchRate_) * filterAlpha;

        float effectiveDeadzone = std::clamp(config_.deadzone, 0.01f, 0.25f);

        float filteredTurn = applyDeadzone(filteredTurnRate_, effectiveDeadzone);
        float pitchForAim = config_.invertY ? -filteredPitchRate_ : filteredPitchRate_;
        float filteredPitch = applyDeadzone(pitchForAim, effectiveDeadzone);

        // Integración de velocidad angular con amortiguación
        float moveX = filteredTurn * config_.sensitivityX * dt * CalibrationTuning::kAimSensitivityGain;
        float moveY = filteredPitch * config_.sensitivityY * dt * CalibrationTuning::kAimSensitivityGain;

        targetAimX_ += moveX;
        targetAimY_ += moveY;

        // Limitar dentro del campo visual
        targetAimX_ = std::clamp(targetAimX_, -1.15f, 1.15f);
        targetAimY_ = std::clamp(targetAimY_, -0.90f, 0.90f);

        // Stick derecho como ajuste analógico auxiliar suave
        Sint16 rx = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_RIGHTX);
        Sint16 ry = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_RIGHTY);
        const float stickDeadzone = std::clamp(config_.deadzone, 0.01f, 0.25f);
        const float stickX = applyDeadzone(rx / 32767.0f, stickDeadzone);
        const float stickYRaw = applyDeadzone(ry / 32767.0f, stickDeadzone);
        if (stickX != 0.0f) {
            targetAimX_ += stickX * config_.sensitivityX * dt * 1.5f;
            targetAimX_ = std::clamp(targetAimX_, -1.15f, 1.15f);
        }
        if (stickYRaw != 0.0f) {
            float stickY = stickYRaw;
            if (config_.invertY) stickY = -stickY;
            targetAimY_ += stickY * config_.sensitivityY * dt * 1.5f;
            targetAimY_ = std::clamp(targetAimY_, -0.90f, 0.90f);
        }

    }

    // 3. Filtro de suavizado (Smooth Interpolation)
    float smoothFactor = 1.0f - std::pow(std::clamp(config_.smoothing, 0.0f, 0.90f), dt * 60.0f);
    state_.x += (targetAimX_ - state_.x) * smoothFactor;
    state_.y += (targetAimY_ - state_.y) * smoothFactor;
}
