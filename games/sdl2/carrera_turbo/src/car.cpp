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
    gear_ = 0;                  // Empieza en Neutral (N)
    rpm_ = 900.0f;              // Ralentí
    isStalled_ = false;
    stallTimer_ = 0.0f;
    isDrifting_ = false;
    wheelAngleVisual_ = 0.0f;
    pitch_ = 0.0f;
    roll_ = 0.0f;
    bobPhase_ = 0.0f;
    shiftAnim_ = 0.0f;
    lastThrottle_ = 0.0f;
    lastBrake_ = 0.0f;
}

void Car::hitObstacle() {
    speed_ = std::max(20.0f, speed_ * 0.50f);
    pitch_ = -18.0f;
}

std::string Car::getGearString() const {
    if (gear_ == -1) return "R";
    if (gear_ == 0) return "N";
    return std::to_string(gear_);
}

void Car::shiftUp() {
    if (gear_ < 6) {
        if (gear_ == -1 && speed_ > 5.0f) return; // No pasar de R a N si va rápido
        gear_++;
    }
}

void Car::shiftDown() {
    if (gear_ > -1) {
        // Para entrar a R (Reversa), el coche debe estar casi detenido (< 12 km/h)
        if (gear_ == 0 && std::abs(speed_) > 12.0f) {
            return;
        }
        gear_--;
    }
}

void Car::update(float dt, const InputState& input, float currentCurve, bool manualTransmission) {
    if (dt <= 0.0f) dt = 0.016f;
    dt = std::min(dt, 0.05f);

    lastThrottle_ = input.throttle;
    lastBrake_ = input.brake;

    bool offRoad = isOffRoad();
    float prevSpeed = speed_;

    // =========================================================================
    // 1. GESTIÓN DE MARCHAS Y TRANSMISIÓN (SECUENCIAL O AUTOMÁTICA)
    // =========================================================================
    int prevGear = gear_;

    if (manualTransmission) {
        if (input.shiftUp) {
            shiftUp();
        } else if (input.shiftDown) {
            shiftDown();
        }
    } else {
        // MODO AUTOMÁTICO (MULTIJUGADOR 2P): Marchas 1 a 6 automáticas según velocidad
        if (gear_ <= 0) gear_ = 1;
        if (speed_ > 46.0f && gear_ == 1) gear_ = 2;
        else if (speed_ > 90.0f && gear_ == 2) gear_ = 3;
        else if (speed_ > 135.0f && gear_ == 3) gear_ = 4;
        else if (speed_ > 175.0f && gear_ == 4) gear_ = 5;
        else if (speed_ > 215.0f && gear_ == 5) gear_ = 6;
        else if (speed_ < 35.0f && gear_ == 2) gear_ = 1;
        else if (speed_ < 75.0f && gear_ == 3) gear_ = 2;
        else if (speed_ < 115.0f && gear_ == 4) gear_ = 3;
        else if (speed_ < 155.0f && gear_ == 5) gear_ = 4;
        else if (speed_ < 195.0f && gear_ == 6) gear_ = 5;
    }

    if (gear_ != prevGear) {
        shiftAnim_ = 1.0f; // Animación de mano en palanca y desacople de embrague en cabina
        pitch_ -= (gear_ > prevGear) ? 6.0f : -3.5f;

        // Pérdida realista de velocidad por desacople de embrague durante el cambio:
        // En un cambio normal se pierden entre 5 y 15 km/h según la velocidad y marcha
        if (gear_ > prevGear && speed_ > 20.0f) {
            float speedFrac = std::clamp((speed_ - 20.0f) / 180.0f, 0.0f, 1.0f);
            float speedLoss = 7.0f + speedFrac * 6.5f; // Rango: 7.0 a 13.5 km/h (promedio 10 km/h)
            speed_ = std::max(10.0f, speed_ - speedLoss);
        }
    } else {
        shiftAnim_ = std::max(0.0f, shiftAnim_ - dt * 3.8f);
    }

    // =========================================================================
    // 2. PARÁMETROS REALISTAS DE TRANSMISIÓN (GEAR RATIOS)
    // =========================================================================
    // Marchas: -1 = R, 0 = N, 1 = 1ª, 2 = 2ª, 3 = 3ª, 4 = 4ª, 5 = 5ª, 6 = 6ª
    const float finalDrive = 3.65f;
    float gearRatio = 0.0f;
    switch (gear_) {
        case -1: gearRatio = -3.20f; break; // Reversa
        case  0: gearRatio =  0.00f; break; // Neutral
        case  1: gearRatio =  3.80f; break; // 1ª: Mucha fuerza, corte ~56 km/h
        case  2: gearRatio =  2.20f; break; // 2ª: Fuerza media-alta, corte ~100 km/h
        case  3: gearRatio =  1.50f; break; // 3ª: Fuerza media, corte ~152 km/h
        case  4: gearRatio =  1.15f; break; // 4ª: Crucero veloz, corte ~202 km/h
        case  5: gearRatio =  0.95f; break; // 5ª: Empuje alta velocidad, corte ~240 km/h
        case  6: gearRatio =  0.82f; break; // 6ª: Overdrive velocidad máxima hasta ~250+ km/h
    }

    const float idleRpm = 900.0f;
    const float redlineRpm = 6800.0f;
    const float revLimiterRpm = 7200.0f;

    // Relación de conversión entre velocidad de ruedas (km/h) y RPM del motor
    // Factor escala de rodadura: a 6500 RPM en 1ª (3.8 * 3.65 = 13.87) da ~55 km/h -> factorK = 8.5f
    const float kWheelToRpmFactor = 8.55f;

    // Recuperación de calado del motor
    if (isStalled_) {
        stallTimer_ -= dt;
        rpm_ = std::max(0.0f, rpm_ - 1500.0f * dt);
        // Si el jugador pone Neutral o suelta acelerador tras 1.2 segundos, el motor rearranca
        if (stallTimer_ <= 0.0f || gear_ == 0) {
            isStalled_ = false;
            rpm_ = idleRpm;
        }
    }

    // Cálculo de RPM conectado a la transmisión
    float targetRpm = idleRpm;
    if (gear_ == 0) {
        // En Neutral: el motor gira libre con el acelerador
        targetRpm = idleRpm + input.throttle * (redlineRpm - idleRpm);
    } else {
        float absSpeed = std::abs(speed_);
        float mechanicalRpm = absSpeed * std::abs(gearRatio) * finalDrive * kWheelToRpmFactor;

        // Efecto 1: Si arrancás en marchas altas (3ª, 4ª, 5ª) desde parado sin embrague,
        // las RPM caen por debajo del mínimo y el motor se ahoga / cala
        if (!isStalled_ && gear_ >= 3 && absSpeed < 18.0f && input.throttle > 0.15f) {
            isStalled_ = true;
            stallTimer_ = 1.3f;
            pitch_ = -8.0f; // Tirón brusco al calarse
        }

        if (isStalled_) {
            targetRpm = 100.0f; // Ahogado
        } else {
            // Si el auto rueda por encima de ralentí, el motor sigue las ruedas
            targetRpm = std::max(idleRpm, mechanicalRpm);

            // Si está detenido y acelera en 1ª o 2ª, simula el resbalamiento de embrague inicial
            if (absSpeed < 15.0f && input.throttle > 0.05f) {
                targetRpm = std::max(targetRpm, idleRpm + input.throttle * 3200.0f);
            }
        }
    }

    // Corte de inyección / Rev limiter
    if (targetRpm > revLimiterRpm) {
        targetRpm = redlineRpm - 200.0f; // Rebote de inyección
    }

    // Suavizado dinámico de RPM
    rpm_ += (targetRpm - rpm_) * (1.0f - std::pow(0.0005f, dt));

    // =========================================================================
    // 3. CURVA DE TORQUE REALISTA DEL MOTOR
    // =========================================================================
    // Curva de par deportiva: muy bajo en bajas RPM (<2.500), despunta a partir de 3.800,
    // par máximo a 4.800 - 6.200 RPM, y decae al corte.
    // Esto obliga a bajar a 2ª o 3ª para salir con potencia de las curvas lentas.
    float rpmNorm = std::clamp((rpm_ - idleRpm) / (redlineRpm - idleRpm), 0.0f, 1.15f);
    float engineTorque = 0.0f;
    if (rpmNorm < 0.25f) {
        // Por debajo de ~2.400 RPM el motor no tiene empuje en marchas largas (se ahoga)
        engineTorque = 0.12f + (rpmNorm / 0.25f) * 0.18f; // 0.12 a 0.30
    } else if (rpmNorm < 0.55f) {
        // Rango medio (2.500 a 4.400 RPM)
        float t = (rpmNorm - 0.25f) / 0.30f;
        engineTorque = 0.30f + t * 0.55f; // 0.30 a 0.85
    } else if (rpmNorm <= 0.85f) {
        engineTorque = 1.0f; // Zona dulce de competición (4.500 - 6.300 RPM)
    } else {
        engineTorque = 1.0f - ((rpmNorm - 0.85f) / 0.30f) * 0.35f;
    }

    // =========================================================================
    // 4. VELOCIDAD MÁXIMA FÍSICA POR MARCHA Y FUERZA EN RUEDAS
    // =========================================================================
    float gearMaxPhysicalSpeed = 0.0f;
    if (gear_ != 0) {
        gearMaxPhysicalSpeed = revLimiterRpm / (std::abs(gearRatio) * finalDrive * kWheelToRpmFactor);
    }

    // Fuerza en ruedas proporcional a la relación de marcha
    float driveForce = 0.0f;
    bool clutchEngaged = (shiftAnim_ < 0.45f);

    if (!isStalled_ && gear_ != 0 && input.throttle > 0.02f && clutchEngaged) {
        float totalRatio = std::abs(gearRatio) * finalDrive;
        // En marchas largas (5ª y 6ª), si vas lento el motor carece de desmultiplicación:
        float gearEfficiency = 1.0f;
        if (gear_ >= 5 && std::abs(speed_) < 130.0f) {
            gearEfficiency = std::clamp(std::abs(speed_) / 130.0f, 0.22f, 1.0f);
        } else if (gear_ == 4 && std::abs(speed_) < 80.0f) {
            gearEfficiency = std::clamp(std::abs(speed_) / 80.0f, 0.35f, 1.0f);
        }

        float baseMultiplier = (gear_ == 1) ? 7.2f : (gear_ == 2 ? 6.5f : (gear_ == 3 ? 5.8f : (gear_ == 4 ? 5.0f : 4.4f)));
        driveForce = (engineTorque * input.throttle) * (totalRatio * baseMultiplier) * gearEfficiency;

        float curAbsSpeed = std::abs(speed_);
        if (curAbsSpeed >= gearMaxPhysicalSpeed) {
            driveForce = 0.0f;
        } else if (curAbsSpeed >= gearMaxPhysicalSpeed * 0.96f) {
            float t = (curAbsSpeed - gearMaxPhysicalSpeed * 0.96f) / (gearMaxPhysicalSpeed * 0.04f);
            driveForce *= (1.0f - t);
        }

        if (offRoad) driveForce *= 0.45f;
    }

    // Resistencia aerodinámica natural (Drag cuadrático ~ v^2)
    float speedRatio = std::abs(speed_) / params_.maxSpeed;
    float aeroDrag = speedRatio * speedRatio * 18.0f;
    float rollingResistance = 4.0f + speedRatio * 4.0f;

    // Efecto 4: Freno motor proporcional a la marcha y RPM
    float engineBraking = 0.0f;
    if (gear_ != 0) {
        if (input.throttle < 0.05f) {
            engineBraking = (rpm_ / redlineRpm) * (std::abs(gearRatio) * 6.5f);
            if (rpm_ > 6000.0f) {
                engineBraking *= 2.4f;
                pitch_ -= 6.0f; // Fuerte cabeceo hacia adelante
            }
        }
        // Si el auto va más rápido de lo que permite la marcha en la que está (ej: bajó a 1ª a 100 km/h),
        // el freno motor es descomunal para bajar de inmediato al corte de esa marcha
        if (std::abs(speed_) > gearMaxPhysicalSpeed) {
            float overspeed = std::abs(speed_) - gearMaxPhysicalSpeed;
            engineBraking += overspeed * 18.0f;
            pitch_ -= 10.0f;
        }
    }

    // Integración de aceleración longitudinal
    if (gear_ == -1) {
        // Marcha Atrás: velocidad negativa hasta ~35 km/h
        speed_ += (-driveForce + rollingResistance + aeroDrag) * dt;
        speed_ = std::clamp(speed_, -gearMaxPhysicalSpeed, 0.0f);
    } else if (gear_ == 0) {
        // Neutral: No hay fuerza de tracción, el auto desacelera por inercia
        speed_ = std::max(0.0f, speed_ - (rollingResistance + aeroDrag) * dt);
    } else {
        // Marchas 1 a 5 hacia adelante
        speed_ += (driveForce - rollingResistance - aeroDrag - engineBraking) * dt;
        if (speed_ < 0.0f) speed_ = 0.0f;
        // Limitación física estricta de la marcha (ej: en 1ª no puede superar ~57 km/h al corte)
        if (speed_ > gearMaxPhysicalSpeed) {
            speed_ = std::max(gearMaxPhysicalSpeed, speed_ - 45.0f * dt);
        }
    }

    // Frenado de pedal progresivo
    if (input.brake > 0.05f) {
        float pedalFactor = input.brake * input.brake;
        float brakeRate = params_.brakingPower * pedalFactor * (0.85f + speedRatio * 0.35f);
        if (speed_ > 0.0f) {
            speed_ = std::max(0.0f, speed_ - brakeRate * dt);
        } else if (speed_ < 0.0f) {
            speed_ = std::min(0.0f, speed_ + brakeRate * dt);
        }
    }

    // Fricción fuera de pista
    if (offRoad && std::abs(speed_) > 35.0f) {
        if (speed_ > 0.0f) speed_ = std::max(35.0f, speed_ - params_.offRoadDecel * dt);
        else speed_ = std::min(-35.0f, speed_ + params_.offRoadDecel * dt);
    }

    // Límite absoluto del vehículo
    float effectiveMaxSpeed = offRoad ? params_.maxSpeed * 0.55f : params_.maxSpeed;
    speed_ = std::clamp(speed_, -38.0f, effectiveMaxSpeed);

    // =========================================================================
    // 5. DIRECCIÓN, SUSPENSIÓN Y EFECTOS POV
    // =========================================================================
    float absSpeedRatio = std::abs(speed_) / params_.maxSpeed;


    // Agarre lateral receptivo y progresivo
    float effectiveGrip = params_.grip * 1.35f;
    if (isDrifting_) effectiveGrip *= 1.25f;
    if (offRoad) effectiveGrip *= 0.6f;

    // Desplazamiento lateral por dirección del jugador: giro ágil y fiel al volante
    x_ += input.steering * effectiveGrip * std::max(0.35f, speedRatio) * dt;

    // Fuerza centrífuga física realista: aumenta fuertemente con la velocidad al cuadrado
    // Exige frenar para curvas cerradas; si encaras una curva en 6ª a 200+ km/h te expulsará hacia afuera
    float speedFactor = (speed_ / 130.0f);
    float centrifugal = currentCurve * (speedFactor * speedFactor) * 0.72f;
    x_ -= centrifugal * dt;
    x_ = std::clamp(x_, -2.2f, 2.2f);

    // Suavizado visual del volante para el renderizado (coincide 1:1 con el giro físico del volante)
    float targetVisual = input.physicalAngleDeg;
    if (std::abs(input.physicalAngleDeg) < 0.1f) targetVisual = input.steering * 65.0f;
    wheelAngleVisual_ += (targetVisual - wheelAngleVisual_) * (1.0f - std::pow(0.0005f, dt));

    // 4. Dinámica de Suspensión, Cabeceo (Pitch) y Balanceo (Roll) para POV
    float longitudinalAccel = (speed_ - prevSpeed) / dt;
    float targetPitch = std::clamp(longitudinalAccel * 0.18f, -14.0f, 12.0f);
    if (input.brake > 0.1f && speed_ > 5.0f) {
        targetPitch -= input.brake * 9.5f;
    }
    pitch_ += (targetPitch - pitch_) * (1.0f - std::pow(0.002f, dt));

    float lateralG = (input.steering * speedRatio * 1.4f) - (currentCurve * speedRatio * speedRatio * 0.28f);
    float targetRoll = std::clamp(lateralG * 6.5f, -8.5f, 8.5f);
    roll_ += (targetRoll - roll_) * (1.0f - std::pow(0.002f, dt));

    // Fase de vibración de alta frecuencia según velocidad y terreno
    float freq = 14.0f + speedRatio * 28.0f + (offRoad ? 25.0f : 0.0f);
    bobPhase_ += freq * dt;
    if (bobPhase_ > 628.318f) bobPhase_ -= 628.318f;

    // 5. Avance longitudinal en el circuito (km/h -> m/s)
    float deltaZ = (speed_ / 3.6f) * dt;
    trackZ_ += deltaZ;
}
