#pragma once

#include <SDL.h>

struct InputState {
    float steering = 0.0f;       // -1.0 (izq) a +1.0 (der)
    float throttle = 0.0f;       // 0.0 a 1.0 (acelerador)
    float brake = 0.0f;          // 0.0 a 1.0 (freno)
    bool handbrake = false;      // Freno de mano / derrape

    float physicalAngleDeg = 0.0f; // Ángulo físico del volante en grados (-90° a +90°)
    bool calibrated = false;

    bool confirm = false;
    bool cancel = false;
    bool pause = false;
};

struct DrivingConfig {
    float steeringSensitivity = 1.0f;
    float deadzone = 0.03f;      // Zona muerta en radianes
    float steeringSmoothing = 0.45f;
    bool rumbleEnabled = true;
};

class InputManager {
public:
    InputManager(DrivingConfig& config);
    ~InputManager();

    bool init();
    void update(float dt);
    void handleEvent(const SDL_Event& ev);

    void calibrate();
    void setRumble(float lowFreq, float highFreq, Uint32 durationMs);
    void setLED(Uint8 r, Uint8 g, Uint8 b);

    const InputState& getState() const { return state_; }
    bool hasController() const { return controller_ != nullptr; }
    bool hasGyro() const { return hasGyro_; }

    void clearMenuFlags();

private:
    void openFirstController();
    void closeController();
    float applyDeadzone(float val, float dz);

    DrivingConfig& config_;
    SDL_GameController* controller_ = nullptr;
    bool hasGyro_ = false;
    bool hasAccel_ = false;

    InputState state_;

    // Orientación física del volante (inclinación lateral / roll)
    float currentRoll_ = 0.0f;   // radianes
    float calibratedRoll_ = 0.0f;
    float targetSteering_ = 0.0f;

    // Entradas de teclado
    bool keyLeft_ = false;
    bool keyRight_ = false;
    bool keyUp_ = false;
    bool keyDown_ = false;
    bool keySpace_ = false;
};
