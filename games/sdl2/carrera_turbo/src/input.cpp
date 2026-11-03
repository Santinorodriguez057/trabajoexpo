#include "input.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

InputManager::InputManager(DrivingConfig& config, int playerIndex)
    : config_(config), playerIndex_(playerIndex) {
}

InputManager::~InputManager() {
    closeController();
}

bool InputManager::init() {
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");

    openAssignedController();
    calibrate();
    return true;
}

void InputManager::openAssignedController() {
    closeController();

    // 1. Asignar DualShock 4 prioritariamente según playerIndex
    // Primero buscamos dispositivos con sensores de movimiento o del fabricante Sony
    int matchedGyroCount = 0;
    int matchedGeneralCount = 0;

    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            SDL_GameController* testCtrl = SDL_GameControllerOpen(i);
            if (testCtrl) {
                bool hasG = SDL_GameControllerHasSensor(testCtrl, SDL_SENSOR_GYRO);
                if (hasG) {
                    if (matchedGyroCount == playerIndex_) {
                        controller_ = testCtrl;
                        hasGyro_ = true;
                        hasAccel_ = SDL_GameControllerHasSensor(controller_, SDL_SENSOR_ACCEL);
                        if (hasGyro_) SDL_GameControllerSetSensorEnabled(controller_, SDL_SENSOR_GYRO, SDL_TRUE);
                        if (hasAccel_) SDL_GameControllerSetSensorEnabled(controller_, SDL_SENSOR_ACCEL, SDL_TRUE);

                        // LED identificador: J1 = Cian Neón, J2 = Rojo / Coral Neón
                        if (playerIndex_ == 0) setLED(0, 220, 255);
                        else setLED(244, 63, 94);
                        break;
                    }
                    matchedGyroCount++;
                }
                SDL_GameControllerClose(testCtrl);
            }
        }
    }

    // Si no se encontró un mando con giroscopio específico para este playerIndex,
    // se intenta abrir cualquier game controller disponible que no sea un arcade stick
    if (!controller_) {
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            if (SDL_IsGameController(i)) {
                if (matchedGeneralCount == playerIndex_) {
                    controller_ = SDL_GameControllerOpen(i);
                    if (controller_) {
                        hasGyro_ = SDL_GameControllerHasSensor(controller_, SDL_SENSOR_GYRO);
                        hasAccel_ = SDL_GameControllerHasSensor(controller_, SDL_SENSOR_ACCEL);
                        if (hasGyro_) SDL_GameControllerSetSensorEnabled(controller_, SDL_SENSOR_GYRO, SDL_TRUE);
                        if (hasAccel_) SDL_GameControllerSetSensorEnabled(controller_, SDL_SENSOR_ACCEL, SDL_TRUE);
                        if (playerIndex_ == 0) setLED(0, 220, 255);
                        else setLED(244, 63, 94);
                        break;
                    }
                }
                matchedGeneralCount++;
            }
        }
    }

    // 2. Si es Player 0 (Modo individual), buscar periférico arcade dedicado (Zero Delay Shifter)
    if (playerIndex_ == 0 && !arcadeStick_) {
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            // Un joystick que no tiene giroscopio o que es reconocido como HID genérico / Zero Delay
            SDL_Joystick* testJoy = SDL_JoystickOpen(i);
            if (testJoy) {
                // Verificar si no es el mismo joystick del controller principal
                if (!controller_ || SDL_JoystickInstanceID(testJoy) != SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller_))) {
                    arcadeStick_ = testJoy;
                    std::printf("[Carrera Turbo] Palanca Arcade / Shifter conectada: %s (ID %d)\n",
                                SDL_JoystickName(arcadeStick_), i);
                    break;
                } else {
                    SDL_JoystickClose(testJoy);
                }
            }
        }
    }
}

void InputManager::closeController() {
    if (controller_) {
        setLED(0, 0, 0);
        SDL_GameControllerClose(controller_);
        controller_ = nullptr;
        hasGyro_ = false;
        hasAccel_ = false;
    }
    if (arcadeStick_) {
        SDL_JoystickClose(arcadeStick_);
        arcadeStick_ = nullptr;
    }
}

void InputManager::calibrate() {
    // Valores fijos pre-calibrados del volante físico (Medio: Accel X = -9.67, Accel Z = 1.39)
    // atan2(1.390683, -9.670010) = 2.999f rad (171.8°)
    const float kFixedCenterAngle = 2.999f;

    if (controller_ && hasAccel_) {
        // CALIBRACIÓN PROMEDIADA: se toman ~25 lecturas durante ~0.25s y se promedia el
        // vector de gravedad. Si el mando se movió durante la toma (mucha dispersión),
        // se conserva el centro anterior para no dejar el volante torcido.
        const int kSamples = 25;
        float sumX = 0.0f, sumZ = 0.0f;
        float firstAng = 0.0f;
        float minAng = 0.0f, maxAng = 0.0f; // Desvío relativo a la primera lectura (seguro ante el salto ±π)
        int valid = 0;
        for (int i = 0; i < kSamples; ++i) {
            SDL_PumpEvents();
            SDL_GameControllerUpdate();
            float accel[3] = {0, 0, -9.8f};
            if (SDL_GameControllerGetSensorData(controller_, SDL_SENSOR_ACCEL, accel, 3) == 0) {
                sumX += accel[0];
                sumZ += accel[2];
                float a = std::atan2(accel[2], accel[0]);
                if (valid == 0) firstAng = a;
                float d = a - firstAng;
                while (d > 3.14159265f) d -= 6.2831853f;
                while (d < -3.14159265f) d += 6.2831853f;
                minAng = std::min(minAng, d);
                maxAng = std::max(maxAng, d);
                ++valid;
            }
            SDL_Delay(10);
        }
        bool stable = valid >= kSamples / 2 && (maxAng - minAng) < 0.12f; // < ~7° de dispersión
        if (stable) {
            calibratedRoll_ = std::atan2(sumZ, sumX);
        } else if (!state_.calibrated) {
            calibratedRoll_ = (valid > 0) ? std::atan2(sumZ, sumX) : kFixedCenterAngle;
        }
        // Si no fue estable y ya había calibración previa, se mantiene calibratedRoll_ anterior
    } else {
        calibratedRoll_ = kFixedCenterAngle;
    }
    currentRoll_ = 0.0f; // El ángulo filtrado es relativo al centro
    pedalHoldTimer_ = 0.0f;
    targetSteering_ = 0.0f;
    state_.steering = 0.0f;
    state_.calibrated = true;
    setRumble(0.2f, 0.4f, 150);
}

float InputManager::applyDeadzone(float val, float dz) {
    float mag = std::abs(val);
    if (mag < dz) return 0.0f;
    float sign = (val > 0.0f) ? 1.0f : -1.0f;
    return sign * (mag - dz) / (1.0f - dz);
}

bool InputManager::isRumblePriorityActive() const {
    return SDL_GetTicks() < priorityRumbleEndTime_;
}

void InputManager::setRumble(float lowFreq, float highFreq, Uint32 durationMs, bool priority) {
    if (!config_.rumbleEnabled || !controller_) return;

    Uint32 now = SDL_GetTicks();
    if (!priority && now < priorityRumbleEndTime_) {
        return;
    }

    if (priority) {
        priorityRumbleEndTime_ = now + durationMs;
    }

    Uint16 low = static_cast<Uint16>(std::clamp(lowFreq, 0.0f, 1.0f) * 65535.0f);
    Uint16 high = static_cast<Uint16>(std::clamp(highFreq, 0.0f, 1.0f) * 65535.0f);
    SDL_GameControllerRumble(controller_, low, high, durationMs);
}

void InputManager::setLED(Uint8 r, Uint8 g, Uint8 b) {
    if (controller_) {
        SDL_GameControllerSetLED(controller_, r, g, b);
    }
}

void InputManager::clearMenuFlags() {
    state_.confirm = false;
    state_.cancel = false;
    state_.pause = false;
}

void InputManager::handleEvent(const SDL_Event& ev) {
    if (ev.type == SDL_CONTROLLERDEVICEADDED) {
        if (!controller_) openAssignedController();
    } else if (ev.type == SDL_CONTROLLERDEVICEREMOVED) {
        if (controller_ && ev.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller_))) {
            closeController();
        }
    } else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
        // Asegurar que cada instancia solo responde a su propio mando físico
        if (controller_ && ev.cbutton.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller_))) {
            if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) state_.confirm = true;
            else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_START) state_.pause = true;
            else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_Y) {
                calibrate();
            }
        }
    } else if (ev.type == SDL_KEYDOWN) {
        // En teclado: J1 usa WASD / Flechas / Espacio si es Player 0
        if (playerIndex_ == 0) {
            if (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_SPACE) state_.confirm = true;
            if (ev.key.keysym.sym == SDLK_ESCAPE) { state_.cancel = true; state_.pause = true; }
            if (ev.key.keysym.sym == SDLK_c || ev.key.keysym.sym == SDLK_r) calibrate();

            if (ev.key.keysym.sym == SDLK_LEFT || ev.key.keysym.sym == SDLK_a) keyLeft_ = true;
            if (ev.key.keysym.sym == SDLK_RIGHT || ev.key.keysym.sym == SDLK_d) keyRight_ = true;
            if (ev.key.keysym.sym == SDLK_UP || ev.key.keysym.sym == SDLK_w) keyUp_ = true;
            if (ev.key.keysym.sym == SDLK_DOWN || ev.key.keysym.sym == SDLK_s) keyDown_ = true;
            if (ev.key.keysym.sym == SDLK_SPACE) keySpace_ = true;

            // Fallback de teclado para cambio manual (E = Shift Up, Q = Shift Down)
            if (ev.key.keysym.sym == SDLK_e) state_.shiftUp = true;
            if (ev.key.keysym.sym == SDLK_q) state_.shiftDown = true;

            // Fallback de teclado para mecánicas de cabina (N = Nitro, W = Wiper, C/R = Enfriar motor)
            if (ev.key.keysym.sym == SDLK_n) state_.nitroTrigger = true;
            if (ev.key.keysym.sym == SDLK_x) state_.wiperTrigger = true;
            if (ev.key.keysym.sym == SDLK_f) state_.coolEngineTap = true;
        } else if (playerIndex_ == 1 && !controller_) {
            // Teclado alternativo J2 si no tiene mando: IJKL
            if (ev.key.keysym.sym == SDLK_j) keyLeft_ = true;
            if (ev.key.keysym.sym == SDLK_l) keyRight_ = true;
            if (ev.key.keysym.sym == SDLK_i) keyUp_ = true;
            if (ev.key.keysym.sym == SDLK_k) keyDown_ = true;
            if (ev.key.keysym.sym == SDLK_RSHIFT) keySpace_ = true;
        }
    } else if (ev.type == SDL_KEYUP) {
        if (playerIndex_ == 0) {
            if (ev.key.keysym.sym == SDLK_LEFT || ev.key.keysym.sym == SDLK_a) keyLeft_ = false;
            if (ev.key.keysym.sym == SDLK_RIGHT || ev.key.keysym.sym == SDLK_d) keyRight_ = false;
            if (ev.key.keysym.sym == SDLK_UP || ev.key.keysym.sym == SDLK_w) keyUp_ = false;
            if (ev.key.keysym.sym == SDLK_DOWN || ev.key.keysym.sym == SDLK_s) keyDown_ = false;
            if (ev.key.keysym.sym == SDLK_SPACE) keySpace_ = false;
        } else if (playerIndex_ == 1 && !controller_) {
            if (ev.key.keysym.sym == SDLK_j) keyLeft_ = false;
            if (ev.key.keysym.sym == SDLK_l) keyRight_ = false;
            if (ev.key.keysym.sym == SDLK_i) keyUp_ = false;
            if (ev.key.keysym.sym == SDLK_k) keyDown_ = false;
            if (ev.key.keysym.sym == SDLK_RSHIFT) keySpace_ = false;
        }
    }
}

void InputManager::update(float dt) {
    if (dt <= 0.0f) dt = 0.016f;

    float steerInput = 0.0f;
    float throttleInput = 0.0f;
    float brakeInput = 0.0f;
    bool handbrakeInput = keySpace_;

    // 1. Control con Mando DualShock / Arcade
    if (controller_) {
        // RECALIBRACIÓN EN CARRERA: R3 (click del stick derecho) o touchpad centran el volante
        bool recalBtn = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_RIGHTSTICK) != 0;
#if SDL_VERSION_ATLEAST(2, 0, 14)
        recalBtn = recalBtn || SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_TOUCHPAD) != 0;
#endif
        if (recalBtn && !prevRecalBtn_ && hasAccel_) calibrate();
        prevRecalBtn_ = recalBtn;
        if (hasGyro_ && hasAccel_) {
            float gyro[3] = {0, 0, 0};
            float accel[3] = {0, 0, -9.8f};
            SDL_GameControllerGetSensorData(controller_, SDL_SENSOR_GYRO, gyro, 3);
            SDL_GameControllerGetSensorData(controller_, SDL_SENSOR_ACCEL, accel, 3);

            // Ángulo absoluto de inclinación física del volante en el plano X-Z
            // Medio: Accel X = -9.67, Accel Z = 1.39 -> atan2(1.39, -9.67) = 2.999f (171.8°)
            float angleFromAccel = std::atan2(accel[2], accel[0]);
            float deltaAngle = angleFromAccel - calibratedRoll_;
            while (deltaAngle > 3.14159265f) deltaAngle -= 2.0f * 3.14159265f;
            while (deltaAngle < -3.14159265f) deltaAngle += 2.0f * 3.14159265f;

            // La medición directa del acelerómetro terrestre da el ángulo real y absoluto sin deriva
            float targetAngle = -deltaAngle;

            // Fusión continua (Complementary Filter) entre Giroscopio y Acelerómetro:
            // NUNCA congelamos el ángulo al frenar/acelerar en una curva (eso causaba congelamiento y luego
            // teletransportación brusca cuando terminaba el timer). En su lugar, filtramos suavemente sacudidas.
            float accelMag = std::sqrt(accel[0] * accel[0] + accel[1] * accel[1] + accel[2] * accel[2]);
            bool shaking = std::abs(accelMag - 9.81f) > 2.2f;

            float gyroRate = (std::abs(gyro[1]) < 0.08f) ? 0.0f : gyro[1];
            float accelWeight = shaking ? 0.04f : 0.18f;
            float fused = (1.0f - accelWeight) * (currentRoll_ - gyroRate * dt) + accelWeight * targetAngle;
            currentRoll_ += (fused - currentRoll_) * 0.85f;

            // Inclinación física real del volante en grados para visualización del cockpit
            state_.physicalAngleDeg = currentRoll_ * 57.2957795f;

            // Zona muerta central para evitar temblores al ir recto
            float filteredRoll = applyDeadzone(currentRoll_, std::max(config_.deadzone, 0.07f));

            // Giro completo (~0.90 radianes / ~52 grados de recorrido del volante)
            const float kMaxWheelAngle = 0.90f;
            steerInput = (filteredRoll / kMaxWheelAngle) * config_.steeringSensitivity;
        } else {
            // Mando convencional sin sensores de movimiento: control con stick analógico
            Sint16 stickX = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_LEFTX);
            if (std::abs(stickX) > 4500) {
                steerInput = (stickX / 32767.0f) * config_.steeringSensitivity;
                state_.physicalAngleDeg = steerInput * 65.0f;
            }
        }

        // Mapeo ergonómico: L2 = Acelerador, L1 = Freno (Mano izquierda en el mando, mano derecha en la palanca de cambios)
        Sint16 rawL2 = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
        Sint16 rawR2 = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        bool btnL1 = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);

        // Acelerador: L2 como primario (también acepta R2 como fallback clásico si alguien lo usa)
        if (rawL2 > 600) {
            throttleInput = static_cast<float>(rawL2) / 32767.0f;
        } else if (rawR2 > 600) {
            throttleInput = static_cast<float>(rawR2) / 32767.0f;
        }

        // Freno: Botón L1 como primario en el DualShock
        if (btnL1 || SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) {
            brakeInput = 1.0f;
        }
    }

    // 2. Fallback de teclado
    if (keyLeft_) steerInput -= 1.0f;
    if (keyRight_) steerInput += 1.0f;
    if (keyUp_) throttleInput = std::max(throttleInput, 1.0f);
    if (keyDown_) brakeInput = std::max(brakeInput, 1.0f);

    steerInput = std::clamp(steerInput, -1.0f, 1.0f);

    // Suavizado del volante
    float smoothAlpha = 1.0f - std::pow(std::clamp(config_.steeringSmoothing, 0.0f, 0.95f), dt * 60.0f);
    state_.steering += (steerInput - state_.steering) * smoothAlpha;
    state_.throttle = std::clamp(throttleInput, 0.0f, 1.0f);
    state_.brake = std::clamp(brakeInput, 0.0f, 1.0f);
    state_.handbrake = handbrakeInput;

    // 3. Lectura de Palanca Arcade Secuencial y Botones de Mecánicas (Zero Delay USB)
    state_.shiftUp = false;
    state_.shiftDown = false;
    state_.nitroTrigger = false;
    state_.wiperTrigger = false;
    state_.coolEngineTap = false;

    if (shiftCooldown_ > 0.0f) {
        shiftCooldown_ -= dt;
    }

    if (arcadeStick_) {
        // En placas Zero Delay, la palanca se conecta al Eje Y (Axis 1) o al Hat (D-pad)
        int rawY = 0;
        if (SDL_JoystickNumAxes(arcadeStick_) > 1) {
            Sint16 axisY = SDL_JoystickGetAxis(arcadeStick_, 1);
            if (axisY < -15000) rawY = -1;      // Arriba (Shift Up)
            else if (axisY > 15000) rawY = 1;  // Abajo (Shift Down)
        }

        // Si los microswitches se conectaron al conector D-Pad / Hat:
        if (rawY == 0 && SDL_JoystickNumHats(arcadeStick_) > 0) {
            Uint8 hat = SDL_JoystickGetHat(arcadeStick_, 0);
            if (hat & SDL_HAT_UP) rawY = -1;
            else if (hat & SDL_HAT_DOWN) rawY = 1;
        }

        // Solo si los ejes no reportan movimiento, comprobar si se cableó a botones individuales (10 y 11)
        if (rawY == 0 && SDL_JoystickNumButtons(arcadeStick_) >= 12) {
            if (SDL_JoystickGetButton(arcadeStick_, 10) != 0) rawY = -1;
            else if (SDL_JoystickGetButton(arcadeStick_, 11) != 0) rawY = 1;
        }

        // Detección limpia de flanco con anti-rebote (debounce de 0.28s)
        // Solo sube o baja EXACTAMENTE 1 marcha por tirón físico y requiere volver al centro
        if (shiftCooldown_ <= 0.0f) {
            if (rawY == -1 && prevStickY_ == 0) {
                state_.shiftUp = true;
                shiftCooldown_ = 0.28f;
            } else if (rawY == 1 && prevStickY_ == 0) {
                state_.shiftDown = true;
                shiftCooldown_ = 0.28f;
            }
        }
        prevStickY_ = rawY;

        // Lectura de botones auxiliares de acción (Mecánicas solicitadas):
        // Botón 0: Nitro Boost
        // Botón 3: Limpiaparabrisas
        // Botón 8 (y fallback 6): Enfriar motor / Ventilación
        int totalBtns = SDL_JoystickNumButtons(arcadeStick_);

        // Imprimir en terminal si se pulsa cualquier botón del stick para verificar ID físico
        for (int b = 0; b < totalBtns; ++b) {
            if (SDL_JoystickGetButton(arcadeStick_, b) != 0) {
                static int lastReportedBtn = -1;
                if (lastReportedBtn != b) {
                    std::printf("[Arcade Stick] Botón presionado: ID %d (total: %d)\n", b, totalBtns);
                    std::fflush(stdout);
                    lastReportedBtn = b;
                }
            }
        }

        if (totalBtns > 0) {
            bool btn0 = (SDL_JoystickGetButton(arcadeStick_, 0) != 0);
            if (btn0 && !prevArcadeBtn0_) state_.nitroTrigger = true;
            prevArcadeBtn0_ = btn0;
        }
        if (totalBtns > 3) {
            bool btn3 = (SDL_JoystickGetButton(arcadeStick_, 3) != 0);
            if (btn3 && !prevArcadeBtn3_) state_.wiperTrigger = true;
            prevArcadeBtn3_ = btn3;
        }
        if (totalBtns > 6) {
            // Mapeado a ID 8 (con fallback a 6 por compatibilidad)
            bool btnCool = (totalBtns > 8 && SDL_JoystickGetButton(arcadeStick_, 8) != 0) ||
                           (SDL_JoystickGetButton(arcadeStick_, 6) != 0);
            if (btnCool && !prevArcadeBtn6_) state_.coolEngineTap = true;
            prevArcadeBtn6_ = btnCool;
        }
    }
}
