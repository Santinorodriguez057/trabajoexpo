#pragma once

#include <SDL.h>
#include "config.h"

struct AimState {
    float x = 0.0f;      // -1.0 a +1.0 (pantalla horizontal)
    float y = 0.0f;      // -1.0 a +1.0 (pantalla vertical)
    float tension = 0.0f;// 0.0 a 1.0 (tension del arco)
    bool isDrawing = false;
    bool justFired = false;
    float releaseTension = 0.0f;
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
    void setRumble(float lowFreq, float highFreq, Uint32 durationMs);
    void setLED(Uint8 r, Uint8 g, Uint8 b);

    const AimState& getState() const { return state_; }
    bool hasController() const { return controller_ != nullptr; }
    bool hasGyro() const { return hasGyro_; }

    // Botones de accion
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

    // Calibracion y sesgo
    float gyroBiasX_ = 0.0f;
    float gyroBiasY_ = 0.0f;
    float gyroBiasZ_ = 0.0f;

    // Estado del gatillo R2
    float lastTriggerValue_ = 0.0f;
    float currentTension_ = 0.0f;
    bool mouseHeld_ = false;
    float mouseTension_ = 0.0f;

    // Flags de interfaz
    bool pausePressed_ = false;
    bool confirmPressed_ = false;
    bool cancelPressed_ = false;
};
