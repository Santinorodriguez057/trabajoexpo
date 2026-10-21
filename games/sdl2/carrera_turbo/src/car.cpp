#include "car.h"
#include <cmath>
#include <algorithm>

Car::Car(const CarPhysics& params)
    : params_(params) {
    reset();
}

void Car::reset() {
    speed_ = 0.0f;
    x_ = 0.0f;
    trackZ_ = 0.0f;
    gear_ = 1;
    rpm_ = 1000.0f;
    isDrifting_ = false;
    wheelAngleVisual_ = 0.0f;
}

void Car::hitObstacle() {
    speed_ = std::max(0.0f, speed_ * 0.45f - 25.0f);
}

void Car::update(float dt, const InputState& input, float currentCurve) {
    if (dt <= 0.0f) dt = 0.016f;

    bool offRoad = isOffRoad();

    // 1. Aceleración y Frenado
    if (input.throttle > 0.05f) {
        float gearFactor = 1.0f - (gear_ - 1) * 0.14f;
        float accelRate = params_.accelPower * gearFactor * input.throttle;
        if (offRoad) accelRate *= 0.4f;

        speed_ += accelRate * dt;
    } else {
        // Desaceleración natural
        speed_ = std::max(0.0f, speed_ - params_.naturalDecel * dt);
    }

    if (input.brake > 0.05f) {
        speed_ = std::max(0.0f, speed_ - params_.brakingPower * input.brake * dt);
    }

    // Fricción extra fuera de pista
    if (offRoad && speed_ > 50.0f) {
        speed_ = std::max(50.0f, speed_ - params_.offRoadDecel * dt);
    }

    // Límite de velocidad
    float effectiveMaxSpeed = offRoad ? params_.maxSpeed * 0.55f : params_.maxSpeed;
    speed_ = std::clamp(speed_, 0.0f, effectiveMaxSpeed);

    // 2. Simulación de Caja de Cambios Automática y RPM
    // 5 marchas escalonadas (0-50, 50-95, 95-145, 145-195, 195-230+)
    const float gearMaxSpeeds[5] = {52.0f, 98.0f, 148.0f, 198.0f, 240.0f};
    const float gearMinSpeeds[5] = {0.0f, 42.0f, 85.0f, 135.0f, 185.0f};

    if (speed_ > gearMaxSpeeds[gear_ - 1] && gear_ < 5) {
        gear_++;
    } else if (gear_ > 1 && speed_ < gearMinSpeeds[gear_ - 1]) {
        gear_--;
    }

    float prevGearSpeed = (gear_ > 1) ? gearMaxSpeeds[gear_ - 2] : 0.0f;
    float curGearRange = gearMaxSpeeds[gear_ - 1] - prevGearSpeed;
    float gearFraction = (speed_ - prevGearSpeed) / std::max(1.0f, curGearRange);
    gearFraction = std::clamp(gearFraction, 0.0f, 1.0f);

    rpm_ = 1200.0f + gearFraction * 5800.0f;
    if (input.throttle > 0.05f && speed_ < 5.0f) {
        rpm_ = 3500.0f + input.throttle * 3000.0f; // Revving en punto muerto
    }

    // 3. Dirección y Fuerza Centrífuga
    float speedRatio = getSpeedRatio();

    // Derrape cuando se gira fuerte a velocidad o con freno de mano
    isDrifting_ = (input.handbrake && speed_ > 35.0f) || (std::abs(input.steering) > 0.75f && speed_ > 140.0f);

    float effectiveGrip = params_.grip;
    if (isDrifting_) effectiveGrip *= 1.35f; // Mayor rotación pero pérdida de velocidad
    if (offRoad) effectiveGrip *= 0.6f;

    // Desplazamiento lateral por dirección del jugador
    x_ += input.steering * effectiveGrip * speedRatio * dt;

    // Fuerza centrífuga empuja hacia el exterior de las curvas
    float centrifugal = currentCurve * speedRatio * speedRatio * params_.centrifugalCoeff;
    x_ -= centrifugal * dt;

    // Suavizado visual del volante para el renderizado
    float targetVisual = input.physicalAngleDeg;
    if (std::abs(targetVisual) < 0.1f) targetVisual = input.steering * 45.0f;
    wheelAngleVisual_ += (targetVisual - wheelAngleVisual_) * (1.0f - std::pow(0.5f, dt * 60.0f));

    // 4. Avance longitudinal en el circuito
    // (speed_ en km/h -> m/s es / 3.6f)
    float deltaZ = (speed_ / 3.6f) * dt;
    trackZ_ += deltaZ;
}
