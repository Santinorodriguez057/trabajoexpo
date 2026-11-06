#include <SDL.h>
#include <SDL_ttf.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>

#include "input.h"
#include "car.h"
#include "road.h"
#include "renderer.h"
#include "sound.h"
#include "ui.h"

static TTF_Font* loadSystemFont(int ptSize) {
    std::vector<std::string> candidates = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    };

    const char* winDir = std::getenv("WINDIR");
    if (!winDir) winDir = std::getenv("SystemRoot");
    std::string winBase = winDir ? winDir : "C:\\Windows";
    candidates.push_back(winBase + "\\Fonts\\arialbd.ttf");
    candidates.push_back(winBase + "\\Fonts\\arial.ttf");
    candidates.push_back(winBase + "\\Fonts\\segoeuib.ttf");
    candidates.push_back(winBase + "\\Fonts\\segoeui.ttf");
    candidates.push_back(winBase + "\\Fonts\\tahoma.ttf");
    candidates.push_back("C:\\Windows\\Fonts\\arial.ttf");

    for (const auto& path : candidates) {
        TTF_Font* f = TTF_OpenFont(path.c_str(), ptSize);
        if (f) return f;
    }
    return nullptr;
}

int main(int argc, char** argv) {
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");

    // Inicialización segura de SDL (reintenta sin AUDIO si el host no tiene servidor de sonido activo)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_SENSOR | SDL_INIT_AUDIO) < 0) {
        std::fprintf(stderr, "Aviso SDL_Init con AUDIO: %s, reintentando sin audio...\n", SDL_GetError());
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_SENSOR) < 0) {
            std::fprintf(stderr, "Error SDL_Init: %s\n", SDL_GetError());
            return 1;
        }
    }

    if (TTF_Init() < 0) {
        std::fprintf(stderr, "Error TTF_Init: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Carrera Turbo — DualShock Steering",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1920, 1080,
        SDL_WINDOW_FULLSCREEN_DESKTOP
    );

    if (!window) {
        std::fprintf(stderr, "Error al crear ventana: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    SDL_Renderer* ren = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!ren) {
        std::fprintf(stderr, "Error al crear renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_RenderSetLogicalSize(ren, 1920, 1080);

    TTF_Font* fontTitle = loadSystemFont(44);
    TTF_Font* fontHud = loadSystemFont(28);
    TTF_Font* fontSmall = loadSystemFont(20);

    DrivingConfig config;
    InputManager input1(config, 0); // Jugador 1 (DualShock 4 como volante o WASD + Fight Stick Shifter)
    input1.init();

    // Nivel de dificultad inicial (por defecto DIFICIL para tener todo disponible, seleccionable al inicio)
    Difficulty difficulty = Difficulty::HARD;

    // Control de transmisión manual: siempre activa (en Fácil no se rompe el motor)
    bool p1ManualTransmission = true;
    bool p1EngineDamageEnabled = (difficulty != Difficulty::EASY);

    SoundManager sound;
    sound.init();

    Car car1;

    Road road;
    // En modo individual Time Attack: pista limpia sin tráfico ni checkpoints tipo arcade
    // Nitro y charcos de barro solo activos en dificultad DIFÍCIL
    road.generateTrack(true, difficulty == Difficulty::HARD);
    road.enableTraffic(false);

    Renderer renderer(ren, fontTitle, fontHud, fontSmall);
    UserInterface ui(renderer, fontTitle, fontHud, fontSmall, config);

    GameState state = GameState::CALIBRATION;
    bool running = true;

    float raceTime = 0.0f;
    float topSpeedAchieved1 = 0.0f;
    int pauseMenuIndex = 0;
    float countdownTimer = 4.0f; // Semáforo estilo F1 (3s de luces rojas progresivas, 1s de verde GO!)
    int lastCountdownStep = 5;

    // =========================================================================
    // VARIABLES DEL MODO TIME ATTACK / VUELTA RÁPIDA (1P)
    // =========================================================================
    const int kTotalLaps = 2;             // Formato Expo: 2 vueltas por jugador (~2.5 min de sesión)
    int currentLap = 1;
    float currentLapTime = 0.0f;
    float bestLapTime = 99999.0f;
    float lastLapTime = 0.0f;
    float prevTrackZ = 0.0f;
    std::vector<float> completedLapTimes;

    // Sectores y deltas (F1 Style)
    int currentSector = 1;
    int completedSectorNotice = 0; // Sector que se acaba de completar (1, 2 o 3) para el cartel
    float currentSectorTime = 0.0f;
    float sectorDelta = 0.0f;
    float sectorBannerTimer = 0.0f;
    float bestSectorTimes[3] = {99999.0f, 99999.0f, 99999.0f};

    Uint32 lastTime = SDL_GetTicks();

    while (running) {
        Uint32 currentTime = SDL_GetTicks();
        float dt = (currentTime - lastTime) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f;
        lastTime = currentTime;

        input1.clearMenuFlags();

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = false;
                break;
            }
            input1.handleEvent(ev);

            // Manejo de eventos de menú
            if (state == GameState::PAUSE) {
                if (ev.type == SDL_KEYDOWN) {
                    if (ev.key.keysym.sym == SDLK_UP) pauseMenuIndex = (pauseMenuIndex + 5) % 6;
                    if (ev.key.keysym.sym == SDLK_DOWN) pauseMenuIndex = (pauseMenuIndex + 1) % 6;
                    if (ev.key.keysym.sym == SDLK_LEFT) {
                        if (pauseMenuIndex == 0) config.steeringSensitivity = std::max(0.4f, config.steeringSensitivity - 0.1f);
                        if (pauseMenuIndex == 1) config.deadzone = std::max(0.0f, config.deadzone - 0.01f);
                        if (pauseMenuIndex == 2) config.steeringSmoothing = std::max(0.0f, config.steeringSmoothing - 0.05f);
                        if (pauseMenuIndex == 3) config.rumbleEnabled = !config.rumbleEnabled;
                    }
                    if (ev.key.keysym.sym == SDLK_RIGHT) {
                        if (pauseMenuIndex == 0) config.steeringSensitivity = std::min(2.5f, config.steeringSensitivity + 0.1f);
                        if (pauseMenuIndex == 1) config.deadzone = std::min(0.2f, config.deadzone + 0.01f);
                        if (pauseMenuIndex == 2) config.steeringSmoothing = std::min(0.9f, config.steeringSmoothing + 0.05f);
                        if (pauseMenuIndex == 3) config.rumbleEnabled = !config.rumbleEnabled;
                    }
                    if (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_SPACE) {
                        if (pauseMenuIndex == 4) state = GameState::RACING;
                        if (pauseMenuIndex == 5) running = false;
                    }
                } else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) pauseMenuIndex = (pauseMenuIndex + 5) % 6;
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) pauseMenuIndex = (pauseMenuIndex + 1) % 6;
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
                        if (pauseMenuIndex == 0) config.steeringSensitivity = std::max(0.4f, config.steeringSensitivity - 0.1f);
                        if (pauseMenuIndex == 1) config.deadzone = std::max(0.0f, config.deadzone - 0.01f);
                        if (pauseMenuIndex == 2) config.steeringSmoothing = std::max(0.0f, config.steeringSmoothing - 0.05f);
                        if (pauseMenuIndex == 3) config.rumbleEnabled = !config.rumbleEnabled;
                    }
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
                        if (pauseMenuIndex == 0) config.steeringSensitivity = std::min(2.5f, config.steeringSensitivity + 0.1f);
                        if (pauseMenuIndex == 1) config.deadzone = std::min(0.2f, config.deadzone + 0.01f);
                        if (pauseMenuIndex == 2) config.steeringSmoothing = std::min(0.9f, config.steeringSmoothing + 0.05f);
                        if (pauseMenuIndex == 3) config.rumbleEnabled = !config.rumbleEnabled;
                    }
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                        if (pauseMenuIndex == 4) state = GameState::RACING;
                        if (pauseMenuIndex == 5) running = false;
                    }
                }
            }
        }

        input1.update(dt);
        const InputState& in1 = input1.getState();

        // Máquina de estados
        static float calibHoldTimer = 0.0f;
        static bool calibHolding = false;

        switch (state) {
            case GameState::CALIBRATION:
                {
                    // Para que no se descalibre rápido:
                    // El jugador debe sostener el mando quieto en el centro manteniendo [X] o [ENTER] durante 0.85s
                    bool pressingConfirm = in1.confirm;
                    if (!pressingConfirm) {
                        // También comprobar directamente el botón A del DualShock o teclado ENTER/ESPACIO
                        if (SDL_GameControllerGetButton(SDL_GameControllerFromInstanceID(0), SDL_CONTROLLER_BUTTON_A) ||
                            SDL_GameControllerGetButton(SDL_GameControllerFromInstanceID(1), SDL_CONTROLLER_BUTTON_A)) {
                            pressingConfirm = true;
                        }
                    }

                    if (pressingConfirm) {
                        calibHolding = true;
                        calibHoldTimer += dt;
                        if (calibHoldTimer >= 0.85f) {
                            // Toma la calibración precisa de gravedad cuando el volante ya está firme en el centro
                            input1.calibrate();
                            sound.playCheckpoint();
                            state = GameState::DIFFICULTY_SELECT;
                            calibHoldTimer = 0.0f;
                            calibHolding = false;
                        }
                    } else {
                        calibHolding = false;
                        calibHoldTimer = std::max(0.0f, calibHoldTimer - dt * 2.0f);
                    }

                    if (in1.cancel) {
                        running = false;
                    }
                }
                break;

            case GameState::DIFFICULTY_SELECT:
                // Navegación con dirección (volante o flechas / D-Pad)
                {
                    static float navCooldown = 0.0f;
                    if (navCooldown > 0.0f) navCooldown -= dt;

                    int curDiff = static_cast<int>(difficulty);
                    if (navCooldown <= 0.0f) {
                        if (in1.steering < -0.45f) {
                            curDiff = std::max(0, curDiff - 1);
                            difficulty = static_cast<Difficulty>(curDiff);
                            sound.playShiftAlert();
                            navCooldown = 0.28f;
                        } else if (in1.steering > 0.45f) {
                            curDiff = std::min(2, curDiff + 1);
                            difficulty = static_cast<Difficulty>(curDiff);
                            sound.playShiftAlert();
                            navCooldown = 0.28f;
                        }
                    }

                    if (in1.confirm) {
                        // Aplicar reglas del nivel de dificultad seleccionado:
                        // Fácil: cambios manuales obligatorios, pero motor indestructible (sin daño por mal cambio)
                        // Media: cambios manuales con daño de motor y enfriamiento [X8]
                        // Difícil: manual con daño + nitro/turbo [X0] + charcos con limpiaparabrisas [X3]
                        p1ManualTransmission = true;
                        p1EngineDamageEnabled = (difficulty != Difficulty::EASY);
                        road.generateTrack(true, difficulty == Difficulty::HARD);

                        countdownTimer = 4.0f;
                        lastCountdownStep = 5;
                        sound.playCountdownBeep(false);
                        state = GameState::COUNTDOWN;
                    } else if (in1.cancel) {
                        state = GameState::CALIBRATION;
                    }
                }
                break;

            case GameState::COUNTDOWN:
                countdownTimer -= dt;
                {
                    int curStep = static_cast<int>(std::ceil(countdownTimer));
                    if (curStep != lastCountdownStep && curStep >= 1) {
                        lastCountdownStep = curStep;
                        if (curStep == 1) {
                            sound.playCountdownBeep(true); // ¡Luz verde / GO!
                        } else {
                            sound.playCountdownBeep(false); // Luz roja
                        }
                    }
                }
                // Los motores rugen en ralentí / aceleración en punto muerto mientras esperan
                sound.updateEngine(car1.getRpm(), in1.throttle);
                if (countdownTimer <= 0.0f) {
                    state = GameState::RACING;
                }
                break;

            case GameState::RACING:
                if (in1.pause) {
                    state = GameState::PAUSE;
                    pauseMenuIndex = 0;
                    break;
                }
                if (in1.cancel) {
                    running = false;
                    break;
                }

                raceTime += dt;
                if (sectorBannerTimer > 0.0f) sectorBannerTimer -= dt;

                currentLapTime += dt;
                currentSectorTime += dt;

                if (car1.getSpeed() > topSpeedAchieved1) topSpeedAchieved1 = car1.getSpeed();

                // Actualizar Carretera
                road.update(dt);

                // Actualizar P1
                {
                    float curve1 = road.getInterpolatedCurve(car1.getTrackZ());
                    const RoadSegment& curSeg1 = road.findSegment(car1.getTrackZ());
                    car1.update(dt, in1, curve1, p1ManualTransmission, p1EngineDamageEnabled);

                    // DEBUG TEMPORAL: telemetría por consola con CT_DEBUG=1
                    {
                        static bool dbg = (std::getenv("CT_DEBUG") != nullptr);
                        static float dbgT = 0.0f;
                        dbgT += dt;
                        if (dbg && dbgT >= 0.5f) {
                            dbgT = 0.0f;
                            std::printf("[DBG] seg=%d curve=%.2f speed=%.0f x=%.2f steer=%.2f wheelDeg=%.1f\n",
                                        curSeg1.index, curSeg1.curve, car1.getSpeed(), car1.getX(),
                                        in1.steering, in1.physicalAngleDeg);
                            std::fflush(stdout);
                        }
                    }

                    float trackLength = road.getTrackLength();

                    // GESTIÓN DE ENTRADAS DE MECÁNICAS SEGÚN DIFICULTAD:
                    // Fácil: sin mecánicas del fight stick
                    // Media: mal cambio y enfriamiento de motor
                    // Difícil: todo lo anterior + Nitro y Limpiaparabrisas
                    if (difficulty == Difficulty::HARD) {
                        if (in1.nitroTrigger) {
                            if (car1.getNitroInventory() > 0 && !car1.isNitroActive()) {
                                car1.triggerNitro();
                                sound.playNitro();
                                input1.setRumble(0.8f, 0.9f, 400);
                                input1.setLED(14, 165, 233); // Celeste eléctrico Nitro
                            }
                        }
                        if (in1.wiperTrigger) {
                            car1.triggerWiper();
                            sound.playWiper();
                        }
                    }

                    if (difficulty != Difficulty::EASY) {
                        if (in1.coolEngineTap) {
                            if (car1.getCoolEngineTapsNeeded() > 0) {
                                car1.tapCoolEngine();
                                sound.playEngineCoolTap();
                                input1.setRumble(0.4f, 0.6f, 100);
                            }
                        }

                        // Sonido y LED de alarma cuando el motor está roto
                        if (car1.isEngineBlown()) {
                            static float alarmSoundTimer = 0.0f;
                            alarmSoundTimer -= dt;
                            if (alarmSoundTimer <= 0.0f) {
                                sound.playEngineAlarm();
                                alarmSoundTimer = 0.5f;
                                input1.setLED(239, 68, 68);
                                input1.setRumble(0.7f, 0.4f, 200);
                            }
                        }
                    }

                    // =========================================================================
                    // LÓGICA DE VUELTA RÁPIDA / TIME ATTACK (1P)
                    // =========================================================================
                    // Sectores divididos en tercios del circuito:
                    // Sector 1: 0 a 1/3
                    // Sector 2: 1/3 a 2/3
                    // Sector 3: 2/3 a Meta
                    float s1Limit = trackLength * 0.3333f;
                    float s2Limit = trackLength * 0.6666f;

                        // Al cruzar el límite del Sector 1, completamos Sector 1 y entramos a Sector 2
                        if (prevTrackZ < s1Limit && car1.getTrackZ() >= s1Limit) {
                            float delta = (bestSectorTimes[0] < 9990.0f) ? (currentSectorTime - bestSectorTimes[0]) : 0.0f;
                            sectorDelta = delta;
                            completedSectorNotice = 1; // Avisar que se completó el Sector 1
                            sectorBannerTimer = 2.4f;
                            if (currentSectorTime < bestSectorTimes[0]) bestSectorTimes[0] = currentSectorTime;
                            currentSectorTime = 0.0f;
                            currentSector = 2; // Ahora estamos en el Sector 2
                            sound.playCheckpoint();
                            if (delta <= 0.0f) {
                                input1.setLED(34, 197, 94); // Verde (más rápido)
                                input1.setRumble(0.4f, 0.6f, 150);
                            } else {
                                input1.setLED(239, 68, 68); // Rojo (más lento)
                                input1.setRumble(0.6f, 0.2f, 150);
                            }
                        } else if (prevTrackZ < s2Limit && car1.getTrackZ() >= s2Limit) {
                            // Al cruzar el límite del Sector 2, completamos Sector 2 y entramos a Sector 3
                            float delta = (bestSectorTimes[1] < 9990.0f) ? (currentSectorTime - bestSectorTimes[1]) : 0.0f;
                            sectorDelta = delta;
                            completedSectorNotice = 2; // Avisar que se completó el Sector 2
                            sectorBannerTimer = 2.4f;
                            if (currentSectorTime < bestSectorTimes[1]) bestSectorTimes[1] = currentSectorTime;
                            currentSectorTime = 0.0f;
                            currentSector = 3; // Ahora estamos en el Sector 3
                            sound.playCheckpoint();
                            if (delta <= 0.0f) {
                                input1.setLED(34, 197, 94);
                                input1.setRumble(0.4f, 0.6f, 150);
                            } else {
                                input1.setLED(239, 68, 68);
                                input1.setRumble(0.6f, 0.2f, 150);
                            }
                        }

                        // Cruce de Meta / Finalización de Vuelta
                        if (car1.getTrackZ() >= trackLength) {
                            // Registrar tiempo de vuelta
                            lastLapTime = currentLapTime;
                            completedLapTimes.push_back(lastLapTime);

                            // Vuelta 1 = Reconocimiento (familiarizarse con el trazado)
                            // Vuelta 2 = Vuelta Cronometrada Oficial (establece el tiempo de récord)
                            if (currentLap == 1) {
                                // Fin de reconocimiento: vibración suave y aviso
                                input1.setLED(250, 204, 21); // Amarillo: alerta de inicio de vuelta oficial
                                input1.setRumble(0.4f, 0.4f, 250);
                            } else {
                                if (lastLapTime < bestLapTime) {
                                    bestLapTime = lastLapTime;
                                    // Récord de vuelta cronometrada: destello LED fucsia/púrpura y vibración de victoria
                                    input1.setLED(168, 85, 247);
                                    input1.setRumble(0.85f, 0.85f, 400);
                                } else {
                                    input1.setRumble(0.5f, 0.5f, 250);
                                }
                            }

                            sound.playCheckpoint();
                            currentLap++;

                            // Si completó las 2 vueltas (Reconocimiento + Cronometrada), finaliza la sesión
                            if (currentLap > kTotalLaps) {
                                state = GameState::FINISH;
                                break;
                            } else {
                                // Continuar circuito cerrado a la siguiente vuelta lanzada sin perder velocidad
                                car1.addDistance(-trackLength);
                                currentLapTime = 0.0f;
                                currentSectorTime = 0.0f;
                                completedSectorNotice = 0; // Mostrar "¡VUELTA CRONOMETRADA INICIADA!"
                                sectorBannerTimer = 3.0f;
                                sectorDelta = 0.0f;
                                currentSector = 1; // Volvemos a iniciar en Sector 1
                            }
                        }

                        prevTrackZ = car1.getTrackZ();

                    // INTERACCIÓN DE P1 CON PROPS INTERACTIVOS EN LA PISTA (Botellas de Nitro y Charcos de Barro):
                    int segIdx = static_cast<int>(std::floor(car1.getTrackZ() / Road::kSegmentLength)) % road.getSegmentCount();
                    if (segIdx >= 0 && segIdx < road.getSegmentCount()) {
                        auto& seg = road.getSegments()[segIdx];
                        for (auto it = seg.props.begin(); it != seg.props.end(); ) {
                            if (it->type == PropType::NITRO_BOTTLE && std::abs(it->offset - car1.getX()) < 0.55f) {
                                // Agarrar botella de nitro
                                car1.addNitroBottle();
                                sound.playCheckpoint();
                                input1.setLED(14, 165, 233);
                                input1.setRumble(0.5f, 0.5f, 150);
                                it = seg.props.erase(it); // Se recoge la botella
                            } else if (it->type == PropType::MUD_PUDDLE && std::abs(it->offset - car1.getX()) < 0.65f) {
                                // Pisar charco de barro -> salpicadura en parabrisas y pérdida ligera de agarre
                                car1.addMudSplatter(0.40f);
                                input1.setRumble(0.4f, 0.7f, 180);
                                sound.playCrash();
                                ++it;
                            } else {
                                ++it;
                            }
                        }
                    }
                }

                // Audio del motor
                sound.updateEngine(car1.getRpm(), in1.throttle);
                sound.playScreech(car1.isDrifting());

                // Háptica y Alerta Sonora para cambio de marcha en P1
                {
                    float rpm1 = car1.getRpm();
                    int gear1 = car1.getGear();

                    // Alerta interactiva de cambio de marcha:
                    // Cuando el auto alcanza las RPM óptimas para pasar a la siguiente marcha (5.800 a 6.700 RPM en marchas 1..5)
                    static float shiftBeepCooldown = 0.0f;
                    if (shiftBeepCooldown > 0.0f) shiftBeepCooldown -= dt;

                    bool nearShiftPoint = (gear1 >= 1 && gear1 < 6 && rpm1 >= 5800.0f && in1.throttle > 0.4f);

                    if (input1.isRumblePriorityActive()) {
                    } else if (car1.isOffRoad() && car1.getSpeed() > 20.0f) {
                        input1.setRumble(0.28f, 0.38f, 60);
                    } else if (car1.isDrifting()) {
                        input1.setRumble(0.20f, 0.30f, 60);
                    } else if (nearShiftPoint) {
                        // VIBRACIÓN FUERTE Y RÍTMICA INTERACTIVA DE CAMBIO DE MARCHA
                        input1.setRumble(0.75f, 0.95f, 75);
                        if (shiftBeepCooldown <= 0.0f) {
                            sound.playShiftAlert();
                            shiftBeepCooldown = 0.22f; // Beep periódico de alerta
                        }
                    } else if (car1.getSpeed() > 5.0f || gear1 == 0) {
                        int g = std::clamp(gear1, 1, 6);
                        float gearBase = (g - 1) * 0.010f;
                        float rpmNorm = std::clamp((rpm1 - 900.0f) / 5900.0f, 0.0f, 1.0f);
                        float lowFreq = 0.020f + gearBase + (in1.throttle * 0.025f) + (rpmNorm * 0.025f);
                        float highFreq = 0.015f + (gearBase * 0.7f) + (car1.getSpeedRatio() * 0.035f);
                        input1.setRumble(lowFreq, highFreq, 45);
                    }

                    // LED identificador según zona de tacómetro
                    if (nearShiftPoint) {
                        // Parpadeo / Rojo intenso brillante cuando está por cambiar
                        input1.setLED(255, 0, 80);
                    } else if (rpm1 < 3800.0f) {
                        input1.setLED(34, 197, 94);   // Verde (bajas RPM)
                    } else if (rpm1 < 5200.0f) {
                        input1.setLED(250, 204, 21);  // Amarillo (zona media)
                    } else if (rpm1 < 5800.0f) {
                        input1.setLED(249, 115, 22);  // Naranja (subiendo)
                    } else {
                        input1.setLED(239, 68, 68);   // Rojo (corte)
                    }
                }
                break;

            case GameState::FINISH:
            case GameState::GAME_OVER:
                sound.playScreech(false);
                if (in1.confirm) {
                    car1.reset();
                    raceTime = 0.0f;

                    // Reinicio variables Time Attack
                    currentLap = 1;
                    currentLapTime = 0.0f;
                    currentSectorTime = 0.0f;
                    lastLapTime = 0.0f;
                    prevTrackZ = 0.0f;
                    sectorBannerTimer = 0.0f;
                    completedLapTimes.clear();

                    state = GameState::RACING;
                } else if (in1.cancel) {
                    running = false;
                }
                break;

            case GameState::PAUSE:
                sound.playScreech(false);
                if (in1.cancel) {
                    state = GameState::RACING;
                }
                break;
        }

        // Renderizado del Juego (Siempre 1P en pantalla completa)
        renderer.renderGame(road, car1, raceTime);

        // Renderizado de Interfaces (UI)
        if (state == GameState::CALIBRATION) {
            float holdProgress = calibHoldTimer / 0.85f;
            ui.renderCalibration(in1, 1, false, holdProgress, calibHolding);
        } else if (state == GameState::DIFFICULTY_SELECT) {
            ui.renderDifficultySelect(difficulty);
        } else if (state == GameState::COUNTDOWN) {
            ui.renderTimeAttackHud(car1, road, currentLap, kTotalLaps, currentLapTime, bestLapTime, lastLapTime, currentSector, sectorDelta, sectorBannerTimer, completedSectorNotice, difficulty);
            ui.renderCountdown(countdownTimer);
        } else if (state == GameState::RACING) {
            ui.renderTimeAttackHud(car1, road, currentLap, kTotalLaps, currentLapTime, bestLapTime, lastLapTime, currentSector, sectorDelta, sectorBannerTimer, completedSectorNotice, difficulty);
        } else if (state == GameState::FINISH) {
            ui.renderTimeAttackFinish(bestLapTime, raceTime, topSpeedAchieved1, completedLapTimes);
        } else if (state == GameState::GAME_OVER) {
            ui.renderTimeAttackFinish(bestLapTime, raceTime, topSpeedAchieved1, completedLapTimes);
        } else if (state == GameState::PAUSE) {
            ui.renderPause(pauseMenuIndex);
        }

        SDL_RenderPresent(ren);
        SDL_Delay(8);
    }

    if (fontTitle) TTF_CloseFont(fontTitle);
    if (fontHud) TTF_CloseFont(fontHud);
    if (fontSmall) TTF_CloseFont(fontSmall);

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
