#pragma once

#include "input.h"

struct CarPhysics {
    // Parámetros ajustables
    float maxSpeed = 230.0f;       // km/h
    float accelPower = 95.0f;      // Tasa de aceleración
    float brakingPower = 180.0f;   // Tasa de frenado
    float naturalDecel = 30.0f;    // Resistencia al aire
    float offRoadDecel = 120.0f;   // Fricción en césped
    float grip = 3.6f;             // Agarre lateral en curvas
    float centrifugalCoeff = 0.35f;// Fuerza centrífuga en curvas
};

class Car {
public:
    Car(const CarPhysics& params = CarPhysics{});

    void update(float dt, const InputState& input, float currentCurve);
    void reset();
    void hitObstacle();

    // Getters
    float getSpeed() const { return speed_; }       // km/h
    float getSpeedRatio() const { return speed_ / params_.maxSpeed; }
    float getX() const { return x_; }               // Posición transversal (-1 a +1 = pista)
    float getTrackZ() const { return trackZ_; }     // Posición longitudinal en pista
    int getGear() const { return gear_; }
    float getRpm() const { return rpm_; }
    bool isOffRoad() const { return std::abs(x_) > 1.05f; }
    bool isDrifting() const { return isDrifting_; }
    float getWheelVisualAngle() const { return wheelAngleVisual_; }

    void addDistance(float delta) { trackZ_ += delta; }

private:
    CarPhysics params_;

    float speed_ = 0.0f;        // km/h
    float x_ = 0.0f;            // -1.0 a +1.0 (0 = centro de pista)
    float trackZ_ = 0.0f;       // Distancia recorrida
    int gear_ = 1;
    float rpm_ = 1000.0f;

    bool isDrifting_ = false;
    float wheelAngleVisual_ = 0.0f; // Grados visuales del volante animado
};
