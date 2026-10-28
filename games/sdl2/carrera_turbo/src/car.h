#pragma once

#include "input.h"
#include <cmath>
#include <algorithm>
#include <string>

struct CarPhysics {
    // Parámetros ajustables
    float maxSpeed = 240.0f;       // km/h
    float accelPower = 58.0f;      // Aceleración suave y progresiva con inercia
    float brakingPower = 110.0f;   // Tasa de frenado base progresiva
    float naturalDecel = 24.0f;    // Desaceleración suave y progresiva
    float offRoadDecel = 90.0f;    // Fricción en césped
    float grip = 3.6f;             // Agarre lateral en curvas
    float centrifugalCoeff = 0.32f;// Fuerza centrífuga en curvas
};

class Car {
public:
    Car(const CarPhysics& params = CarPhysics{});

    void update(float dt, const InputState& input, float currentCurve, bool manualTransmission = false);
    void reset();
    void hitObstacle();
    void shiftUp();
    void shiftDown();

    // Getters principales (100% compatibles con el protocolo original)
    float getSpeed() const { return speed_; }       // km/h
    float getSpeedRatio() const { return speed_ / params_.maxSpeed; }
    float getX() const { return x_; }               // Posición transversal (-1 a +1 = pista)
    float getTrackZ() const { return trackZ_; }     // Posición longitudinal en pista
    int getGear() const { return gear_; }           // -1 = Reversa (R), 0 = Neutral (N), 1 a 6 = Marchas
    std::string getGearString() const;
    float getRpm() const { return rpm_; }
    bool isStalled() const { return isStalled_; }
    bool isOffRoad() const { return std::abs(x_) > 1.05f; }
    bool isDrifting() const { return isDrifting_; }
    float getWheelVisualAngle() const { return wheelAngleVisual_; }

    // Getters de telemetría de cabina inmersiva (suspensión y fuerzas G)
    float getPitch() const { return pitch_; }
    float getRoll() const { return roll_; }
    float getSuspensionBob() const { return bobPhase_; }
    float getShiftAnim() const { return shiftAnim_; }
    float getThrottleInput() const { return lastThrottle_; }
    float getBrakeInput() const { return lastBrake_; }

    void setInitialPosition(float x, float z) { x_ = x; trackZ_ = z; }
    void bumpX(float deltaX) { x_ = std::clamp(x_ + deltaX, -0.95f, 0.95f); }
    void addDistance(float delta) { trackZ_ += delta; }

private:
    CarPhysics params_;

    float speed_ = 0.0f;        // km/h
    float x_ = 0.0f;            // -1.0 a +1.0 (0 = centro de pista)
    float trackZ_ = 0.0f;       // Distancia recorrida
    int gear_ = 0;              // Empieza en Neutral (N): -1 = R, 0 = N, 1 a 6 = Marchas
    float rpm_ = 900.0f;        // Ralentí
    bool isStalled_ = false;    // Motor calado / ahogado
    float stallTimer_ = 0.0f;

    bool isDrifting_ = false;
    float wheelAngleVisual_ = 0.0f; // Grados visuales del volante animado

    // Dinámica de cabina en primera persona
    float pitch_ = 0.0f;        // Cabeceo longitudinal (aceleración / frenado)
    float roll_ = 0.0f;         // Inclinación lateral (fuerza G en curvas)
    float bobPhase_ = 0.0f;     // Fase de vibración del motor y asfalto
    float shiftAnim_ = 0.0f;    // Animación de la mano en la palanca de cambios (0 a 1)
    float lastThrottle_ = 0.0f;
    float lastBrake_ = 0.0f;
};
