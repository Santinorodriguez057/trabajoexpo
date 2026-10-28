#pragma once

#include <SDL.h>
#include "config.h"

struct AimState {
    float x = 0.0f;      // -1.0 a +1.0 (pantalla horizontal)
    float y = 0.0f;      // -1.0 a +1.0 (pantalla vertical)
    bool triggerHeld = false;
    bool justFired = false;
    bool calibrated = false;
};

class InputManager {
public:
    InputManager(GameConfig& config);
    ~InputManager();

    bool init();
    void update(float dt);
    void handleEvent(const SDL_Event& ev);

    void calibrate();
    void recenterAim();
    void setRumble(float lowFreq, float highFreq, Uint32 durationMs);
    void setLED(Uint8 r, Uint8 g, Uint8 b);

    const AimState& getState() const { return state_; }
    bool hasController() const { return controller_ != nullptr; }
    bool hasGyro() const { return hasGyro_; }
    bool hasAccel() const { return hasAccel_; }

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
    // Sesgo de reposo del giroscopio, estimado durante la calibración.
    float gyroBiasX_ = 0.0f;
    float gyroBiasY_ = 0.0f;
    float gyroBiasZ_ = 0.0f;

    // Estado del gatillo y ventana de absorción de vibración al disparar.
    float postShotFreezeTimer_ = 0.0f;
    float rumbleSuppressTimer_ = 0.0f;
    float filteredTurnRate_ = 0.0f;
    float filteredPitchRate_ = 0.0f;
    bool mouseHeld_ = false;

    // Flags de interfaz
    bool pausePressed_ = false;
    bool confirmPressed_ = false;
    bool cancelPressed_ = false;
};
