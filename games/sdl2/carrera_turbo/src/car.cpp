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
    latVel_ = 0.0f;
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
    nitroInventory_ = 1;
    nitroTimer_ = 0.0f;
    mudSplatter_ = 0.0f;
    wiperActive_ = false;
    wiperProgress_ = 0.0f;
    engineOverheat_ = 0.0f;
    coolTapsRemaining_ = 0;
    wrongShiftAlarmTimer_ = 0.0f;
}

void Car::triggerNitro() {
    if (nitroInventory_ > 0 && nitroTimer_ <= 0.0f) {
        nitroInventory_--;
        nitroTimer_ = 1.7f; // Impulso concentrado estilo Champiñón rojo de Mario Kart (~1.7s)
        // Patada instantánea sin importar la marcha:
        // Catapulta la velocidad de inmediato a ~255 km/h o suma +80 km/h si ya iba rápido
        speed_ = std::clamp(std::max(speed_ + 80.0f, 255.0f), 0.0f, 280.0f);
        pitch_ += 10.0f;    // Fuerte tirón hacia atrás por aceleración brutal
    }
}

void Car::triggerWiper() {
    wiperActive_ = true;
    wiperProgress_ = 0.0f;
}

void Car::tapCoolEngine() {
    if (coolTapsRemaining_ > 0) {
        coolTapsRemaining_--;
        if (coolTapsRemaining_ <= 0) {
            engineOverheat_ = 0.0f; // Motor completamente enfriado y recuperado
            wrongShiftAlarmTimer_ = 0.0f;
        } else {
            engineOverheat_ = std::max(0.0f, engineOverheat_ - 0.14f);
        }
    }
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

void Car::update(float dt, const InputState& input, float currentCurve, bool manualTransmission, bool engineDamageEnabled) {
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

        // Si bajó de marcha a una velocidad excesiva para la nueva marcha (ej: bajar a 1ª o 2ª a más de su corte físico)
        // se produce SOBRERRÉGIMEN / MOTOR ROTO (si el daño está habilitado):
        if (engineDamageEnabled && gear_ < prevGear && gear_ > 0) {
            float newGearMax = 60.0f;
            if (gear_ == 1) newGearMax = 58.0f;
            else if (gear_ == 2) newGearMax = 105.0f;
            else if (gear_ == 3) newGearMax = 155.0f;
            else if (gear_ == 4) newGearMax = 205.0f;
            else if (gear_ == 5) newGearMax = 245.0f;

            // Detección estricta y castigadora de sobrerrégimen por mal cambio:
            // Si el jugador baja a una marcha cuando va a más velocidad de la que soporta esa marcha,
            // el motor se revienta (sobrecalentamiento instantáneo al 100%), pierde el 85% de fuerza,
            // la caja se clava y exige 7 pulsaciones de enfriamiento/purga para poder volver a acelerar.
            if (speed_ > newGearMax * 1.01f) {
                engineOverheat_ = 1.0f;
                coolTapsRemaining_ = 7; // Más castigador: requiere 7 toques al botón X6
                wrongShiftAlarmTimer_ = 6.0f;
                pitch_ -= 16.0f; // Bloqueo violento de caja
                speed_ = std::max(20.0f, speed_ * 0.70f); // Frenada brusca por detonación mecánica
            }
        }

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
        case  1: gearRatio =  3.80f; break; // 1ª: Mucha fuerza, corte ~58 km/h
        case  2: gearRatio =  2.30f; break; // 2ª: Fuerza media-alta, corte ~105 km/h
        case  3: gearRatio =  1.60f; break; // 3ª: Fuerza media, corte ~155 km/h
        case  4: gearRatio =  1.20f; break; // 4ª: Crucero veloz, corte ~205 km/h
        case  5: gearRatio =  0.98f; break; // 5ª: Empuje alta velocidad, corte ~245 km/h
        case  6: gearRatio =  0.84f; break; // 6ª: Overdrive velocidad máxima hasta ~270 km/h
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

        // Calado realista del motor por exceso de marcha a baja velocidad (Lugging / Stalling):
        // Solo si vas a velocidad excesivamente baja para la marcha (ej: detenido en 5ª)
        if (!isStalled_ && gear_ >= 5 && absSpeed < 45.0f && input.throttle > 0.05f) {
            isStalled_ = true;
            stallTimer_ = 1.2f;
            pitch_ = -8.0f; // Tirón brusco al calarse
        } else if (!isStalled_ && gear_ == 4 && absSpeed < 32.0f && input.throttle > 0.05f) {
            isStalled_ = true;
            stallTimer_ = 1.0f;
            pitch_ = -7.0f;
        } else if (!isStalled_ && gear_ == 3 && absSpeed < 20.0f && input.throttle > 0.05f) {
            isStalled_ = true;
            stallTimer_ = 0.8f;
            pitch_ = -6.0f;
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
    // Curva de par deportiva con buen rango en altas velocidades
    float rpmNorm = std::clamp((rpm_ - idleRpm) / (redlineRpm - idleRpm), 0.0f, 1.15f);
    float engineTorque = 0.0f;
    if (rpmNorm < 0.22f) {
        // En muy bajas RPM (<1.800 RPM)
        engineTorque = 0.35f + (rpmNorm / 0.22f) * 0.30f; // 0.35 a 0.65
    } else if (rpmNorm < 0.50f) {
        // Rango medio
        float t = (rpmNorm - 0.22f) / 0.28f;
        engineTorque = 0.65f + t * 0.35f; // 0.65 a 1.00
    } else if (rpmNorm <= 0.90f) {
        engineTorque = 1.0f; // Zona de máxima potencia deportiva
    } else {
        engineTorque = 1.0f - ((rpmNorm - 0.90f) / 0.25f) * 0.20f;
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
        float baseMultiplier = 6.4f;
        if (gear_ == 1) baseMultiplier = 7.4f;
        else if (gear_ == 2) baseMultiplier = 6.8f;
        else if (gear_ == 3) baseMultiplier = 6.4f;
        else if (gear_ == 4) baseMultiplier = 6.0f;
        else if (gear_ == 5) baseMultiplier = 5.8f;
        else if (gear_ == 6) baseMultiplier = 5.5f;

        driveForce = (engineTorque * input.throttle) * (totalRatio * baseMultiplier);

        // Si el motor está roto/sobrecalentado, la potencia cae un 88% y la velocidad máxima se ahoga
        if (engineOverheat_ > 0.0f) {
            driveForce *= (1.0f - engineOverheat_ * 0.88f);
        }

        // Impulso adicional de NITRO
        if (nitroTimer_ > 0.0f) {
            driveForce += 650.0f; // Empuje masivo
        }

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

    // 314: Efecto 4: Freno motor proporcional a la marcha y RPM (Anulado mientras sopla el Nitro)
    float engineBraking = 0.0f;
    if (gear_ != 0 && nitroTimer_ <= 0.0f) {
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
        // (A menos que esté activo el impulso de Nitro que empuja sin importar la marcha)
        if (nitroTimer_ > 0.0f) {
            speed_ = std::max(speed_, 255.0f);
        } else {
            speed_ = std::max(0.0f, speed_ - (rollingResistance + aeroDrag) * dt);
        }
    } else {
        // Marchas 1 a 5 hacia adelante
        speed_ += (driveForce - rollingResistance - aeroDrag - engineBraking) * dt;
        if (speed_ < 0.0f) speed_ = 0.0f;
        // Limitación física estricta de la marcha (ej: en 1ª no puede superar ~57 km/h al corte)
        // EXCEPCIÓN: Mientras el nitro esté activo, el auto vuela sin respetar la marcha actual
        if (nitroTimer_ <= 0.0f && speed_ > gearMaxPhysicalSpeed) {
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


    // Agarre lateral deportivo y receptivo
    float effectiveGrip = params_.grip * 2.2f;
    if (isDrifting_) effectiveGrip *= 1.30f;
    if (offRoad) effectiveGrip *= 0.65f;

    // MODELO DE CURVAS JUGABLE:
    // - A baja velocidad (80-120 km/h) el auto tiene buen agarre para doblar limpiamente en 2ª/3ª.
    // - A alta velocidad (>140 km/h), la fuerza centrífuga (que crece al cuadrado) supera el agarre en curvas cerradas,
    //   haciendo imposible pasar a fondo en 5ª marcha y forzando a frenar antes del vértice.
    float speedRatioNorm = std::clamp(speed_ / params_.maxSpeed, 0.0f, 1.0f);
    float steerAuthority = effectiveGrip * (0.35f + 0.65f * speedRatioNorm);
    if (speed_ < 5.0f) steerAuthority *= speed_ / 5.0f; // detenido no se desplaza
    float steerPush = input.steering * steerAuthority;

    // Fuerza centrífuga calibrada: castiga intentar tomar curvas cerradas a más de 135 km/h
    float centrifugal = currentCurve * speedRatioNorm * speedRatioNorm * 2.55f;

    // INERCIA LATERAL: respuesta directa y progresiva en curvas (~0.10s) para que la trazada se sienta 100% conectada al volante
    float targetLatVel = steerPush - centrifugal;
    latVel_ += (targetLatVel - latVel_) * (1.0f - std::pow(0.0002f, dt));
    x_ += latVel_ * dt;
    if (x_ > 2.2f || x_ < -2.2f) latVel_ = 0.0f;
    x_ = std::clamp(x_, -2.2f, 2.2f);

    // Suavizado visual del volante para el renderizado (coincide 1:1 con el giro físico del volante)
    float targetVisual = input.physicalAngleDeg;
    if (std::abs(input.physicalAngleDeg) < 0.1f) targetVisual = input.steering * 65.0f;
    wheelAngleVisual_ += (targetVisual - wheelAngleVisual_) * (1.0f - std::pow(0.0005f, dt));

    // 4. Dinámica de Suspensión, Cabeceo (Pitch) y Balanceo (Roll) para POV
    float longitudinalAccel = (speed_ - prevSpeed) / dt;
    float targetPitch = std::clamp(longitudinalAccel * 0.12f, -8.0f, 8.0f);
    if (input.brake > 0.1f && speed_ > 5.0f) {
        targetPitch -= input.brake * 2.2f; // Cabeceo suave al frenar, sin sacudidas bruscas
    }
    pitch_ += (targetPitch - pitch_) * (1.0f - std::pow(0.005f, dt));

    // Fuerza G lateral e inclinación suave de la carrocería en curvas
    float lateralG = (input.steering * speedRatio * 1.0f) - (currentCurve * speedRatio * 0.20f);
    float targetRoll = std::clamp(lateralG * 4.0f, -7.0f, 7.0f);
    roll_ += (targetRoll - roll_) * (1.0f - std::pow(0.005f, dt));

    // Fase de vibración de alta frecuencia según velocidad y terreno
    float freq = 14.0f + speedRatio * 28.0f + (offRoad ? 25.0f : 0.0f);
    bobPhase_ += freq * dt;
    if (bobPhase_ > 628.318f) bobPhase_ -= 628.318f;

    // 5. Avance longitudinal en el circuito (km/h -> m/s)
    float deltaZ = (speed_ / 3.6f) * dt;
    trackZ_ += deltaZ;

    // Actualización de temporizadores de Nitro, Limpiaparabrisas y Alarma
    if (nitroTimer_ > 0.0f) {
        nitroTimer_ -= dt;
        pitch_ += 2.5f; // Mantiene la trompa levantada mientras ruge el nitro
        // Durante el impulso de nitro mantiene velocidad sostenida de aceleración máxima
        speed_ = std::max(speed_, 250.0f);
    } else {
        // Al terminar el turbo, la velocidad decrece suave y deportivamente hacia la velocidad natural de la marcha
        if (gear_ > 0 && speed_ > gearMaxPhysicalSpeed) {
            speed_ = std::max(gearMaxPhysicalSpeed, speed_ - 75.0f * dt);
        }
    }

    if (wiperActive_) {
        wiperProgress_ += dt * 2.8f; // Ciclo de ida y vuelta rápido (~0.7s)
        // A la mitad del barrido limpia un gran porcentaje del barro
        if (wiperProgress_ >= 0.5f && mudSplatter_ > 0.0f) {
            mudSplatter_ = std::max(0.0f, mudSplatter_ - dt * 2.2f);
        }
        if (wiperProgress_ >= 1.0f) {
            wiperActive_ = false;
            wiperProgress_ = 0.0f;
            mudSplatter_ = 0.0f; // Completamente limpio
        }
    }

    if (wrongShiftAlarmTimer_ > 0.0f) {
        wrongShiftAlarmTimer_ -= dt;
    }
}
