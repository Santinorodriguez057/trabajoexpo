#pragma once

#include <SDL.h>
#include <deque>
#include <utility>
#include "config.h"

struct AimState {
    float x = 0.0f;      // -1.0 a +1.0 (pantalla horizontal)
    float y = 0.0f;      // -1.0 a +1.0 (pantalla vertical)
    bool triggerHeld = false;
    bool justFired = false;
    bool calibrated = false;
};

struct CalibrationTelemetry {
    float gyroSpeed = 0.0f;       // rad/s tras corregir el sesgo estimado
    float rawGyro[3] = {};
    float rawAccel[3] = {};
    float accel[3] = {};
    float quietness = 0.0f;       // 0..1, calculada sobre el promedio móvil
    float rollDegrees = 0.0f;
    float pitchDegrees = 0.0f;
    bool hasGyro = false;
    bool hasAccel = false;
    bool postureValid = false;
};

namespace CalibrationTuning {
constexpr float kQuietGyroRadPerSec = 0.43633231f; // 25 grados/segundo
constexpr float kMinimumQuietness = 0.85f;
constexpr float kQuietnessScoreRangeRadPerSec = kQuietGyroRadPerSec / (1.0f - kMinimumQuietness);
constexpr float kGyroAverageWindowSeconds = 0.20f;
constexpr float kMaxRollDegrees = 35.0f;
constexpr float kComfortPitchMinDegrees = -80.0f;
constexpr float kComfortPitchMaxDegrees = 80.0f;
constexpr float kAccelLowPassSeconds = 0.14f;
constexpr float kMaxCalibrationWaitSeconds = 8.0f;
constexpr float kInvalidProgressDecayRatio = 1.0f / 3.0f;
constexpr float kRecenterHoldSeconds = 1.0f;
constexpr float kAimSensitivityGain = 2.75f;
}

class InputManager {
public:
    InputManager(GameConfig& config);
    ~InputManager();

    bool init();
    void update(float dt);
    void handleEvent(const SDL_Event& ev);

    void calibrate();
    void recenterAim();
    void beginCalibration();
    void accumulateCalibrationSample();
    void completeCalibration();
    void setRumble(float lowFreq, float highFreq, Uint32 durationMs);
    void setLED(Uint8 r, Uint8 g, Uint8 b);

    const AimState& getState() const { return state_; }
    bool hasController() const { return controller_ != nullptr; }
    bool hasGyro() const { return hasGyro_; }
    bool hasAccel() const { return hasAccel_; }
    const CalibrationTelemetry& getCalibrationTelemetry() const { return telemetry_; }
    float getRecenterHoldProgress() const { return recenterHoldProgress_; }
    bool takeRecenterRequest();

    // Telemetría de sensores para calibración y diagnóstico
    float getPitchDegrees() const { return currentPitchAngle_ * 57.2957795f; }
    float getYawDegrees() const { return currentYawAngle_ * 57.2957795f; }
    float getRollDegrees() const { return currentRollAngle_ * 57.2957795f; }

    // Botones de acción
    bool pausePressed() const { return pausePressed_; }
    bool confirmPressed() const { return confirmPressed_; }
    bool cancelPressed() const { return cancelPressed_; }
    void clearActionFlags();

private:
    void openFirstController();
    void closeController();
    float applyDeadzone(float value, float deadzone);

    GameConfig& config_;
    SDL_GameController* controller_ = nullptr;
    bool hasGyro_ = false;
    bool hasAccel_ = false;
    CalibrationTelemetry telemetry_;
    std::deque<std::pair<float, float>> gyroSpeedSamples_; // duration, speed
    float gyroSpeedSampleSeconds_ = 0.0f;
    float filteredAccel_[3] = {};
    bool accelFilterInitialized_ = false;

    AimState state_;
    float targetAimX_ = 0.0f;
    float targetAimY_ = 0.0f;

    // Orientación física absoluta calculada con fusión de sensores (AHRS)
    float currentPitchAngle_ = 0.0f; // radianes
    float currentYawAngle_ = 0.0f;   // radianes
    float currentRollAngle_ = 0.0f;  // radianes

    // Referencias de calibración (posición de tiro cómoda del usuario)
    float calibratedPitch_ = 0.0f;
    float calibratedRoll_ = 0.0f;
    float calibratedYaw_ = 0.0f;
    float calibratedAccel_[3] = {};
    // Sesgo de reposo del giroscopio, estimado durante la calibración.
    float gyroBiasX_ = 0.0f;
    float gyroBiasY_ = 0.0f;
    float gyroBiasZ_ = 0.0f;
    double calibrationGyroSum_[3] = {};
    int calibrationGyroSamples_ = 0;

    // Estado del gatillo y ventana de absorción de vibración al disparar.
    float postShotFreezeTimer_ = 0.0f;
    float rumbleSuppressTimer_ = 0.0f;
    float filteredTurnRate_ = 0.0f;
    float filteredPitchRate_ = 0.0f;
    bool mouseHeld_ = false;
    bool recenterRequested_ = false;
    bool recenterLatched_ = false;
    bool recenterKeyDown_ = false;
    float recenterHoldProgress_ = 0.0f;

    // Flags de interfaz
    bool pausePressed_ = false;
    bool confirmPressed_ = false;
    bool cancelPressed_ = false;
};
