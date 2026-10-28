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

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_SENSOR | SDL_INIT_AUDIO) < 0) {
        std::fprintf(stderr, "Error SDL_Init: %s\n", SDL_GetError());
        return 1;
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
    TTF_Font* fontSmall = loadSystemFont(20);    DrivingConfig config;
    InputManager input1(config, 0); // Jugador 1 (Mando 1 o WASD)
    input1.init();

    InputManager input2(config, 1); // Jugador 2 (Mando 2 o IJKL)
    input2.init();

    // Detección de mandos: se activa pantalla dividida SOLO si hay 2 mandos DualShock conectados
    // Si hay 1 DualShock + 1 placa Arcade (Zero Delay), el modo es INDIVIDUAL (DualShock volante + Arcade Shifter)
    bool isSplitScreen = (input1.hasController() && input2.hasController() && input1.hasGyro() && input2.hasGyro());

    // Si hay una palanca arcade conectada o estamos en 1P, activamos transmisión manual secuencial para P1
    bool p1ManualTransmission = !isSplitScreen;

    SoundManager sound;
    sound.init();

    Car car1;
    Car car2;

    if (isSplitScreen) {
        car1.setInitialPosition(-0.35f, 0.0f);
        car2.setInitialPosition( 0.35f, 0.0f);
    }

    Road road;
    // En modo individual (1P Time Attack), pista limpia sin tráfico ni arcos de checkpoint arcade
    road.generateTrack(!isSplitScreen);
    road.enableTraffic(isSplitScreen);

    Renderer renderer(ren, fontTitle, fontHud, fontSmall);
    UserInterface ui(renderer, fontTitle, fontHud, fontSmall, config);

    GameState state = GameState::CALIBRATION;
    bool running = true;

    float raceTime = 0.0f;
    float timeRemaining = 45.0f; // 45 segundos para el primer checkpoint (solo 2P)
    int checkpointsPassed1 = 0;
    int checkpointsPassed2 = 0;
    int nextCheckpointSegment1 = 60;
    int nextCheckpointSegment2 = 60;
    float bonusBannerTimer1 = 0.0f;
    float bonusBannerTimer2 = 0.0f;
    float topSpeedAchieved1 = 0.0f;
    float topSpeedAchieved2 = 0.0f;
    int winnerPlayer = 0; // 0 = sin ganador, 1 = P1, 2 = P2
    int pauseMenuIndex = 0;

    // =========================================================================
    // VARIABLES DEL MODO TIME ATTACK / VUELTA RÁPIDA (SOLO 1P)
    // =========================================================================
    const int kTotalLaps = 3;             // 3 Vueltas por sesión
    int currentLap = 1;
    float currentLapTime = 0.0f;
    float bestLapTime = 99999.0f;
    float lastLapTime = 0.0f;
    float prevTrackZ = 0.0f;
    std::vector<float> completedLapTimes;

    // Sectores y deltas (F1 Style)
    int currentSector = 1;
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
        if (isSplitScreen) input2.clearMenuFlags();

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = false;
                break;
            }
            input1.handleEvent(ev);
            if (isSplitScreen) input2.handleEvent(ev);

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

        InputState in2;
        if (isSplitScreen) {
            input2.update(dt);
            in2 = input2.getState();
        }

        // Máquina de estados
        switch (state) {
            case GameState::CALIBRATION:
                if (in1.confirm || (isSplitScreen && in2.confirm)) {
                    input1.calibrate();
                    if (isSplitScreen) input2.calibrate();
                    state = GameState::RACING;
                } else if (in1.cancel || (isSplitScreen && in2.cancel)) {
                    running = false;
                }
                break;

            case GameState::RACING:
                if (in1.pause || (isSplitScreen && in2.pause)) {
                    state = GameState::PAUSE;
                    pauseMenuIndex = 0;
                    break;
                }
                if (in1.cancel || (isSplitScreen && in2.cancel)) {
                    running = false;
                    break;
                }

                raceTime += dt;
                if (bonusBannerTimer1 > 0.0f) bonusBannerTimer1 -= dt;
                if (bonusBannerTimer2 > 0.0f) bonusBannerTimer2 -= dt;
                if (sectorBannerTimer > 0.0f) sectorBannerTimer -= dt;

                if (isSplitScreen) {
                    timeRemaining -= dt;
                } else {
                    currentLapTime += dt;
                    currentSectorTime += dt;
                }

                if (car1.getSpeed() > topSpeedAchieved1) topSpeedAchieved1 = car1.getSpeed();
                if (isSplitScreen && car2.getSpeed() > topSpeedAchieved2) topSpeedAchieved2 = car2.getSpeed();

                // Actualizar Carretera
                road.update(dt);

                // Actualizar P1
                {
                    const RoadSegment& curSeg1 = road.findSegment(car1.getTrackZ());
                    car1.update(dt, in1, curSeg1.curve, p1ManualTransmission);

                    float trackLength = road.getTrackLength();

                    if (!isSplitScreen) {
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

                            if (lastLapTime < bestLapTime) {
                                bestLapTime = lastLapTime;
                                // Récord de vuelta: destello LED fucsia/púrpura y vibración de victoria
                                input1.setLED(168, 85, 247);
                                input1.setRumble(0.85f, 0.85f, 400);
                            } else {
                                input1.setRumble(0.5f, 0.5f, 250);
                            }

                            sound.playCheckpoint();
                            currentLap++;

                            // Si completó las 3 vueltas fijas de la sesión, finaliza la carrera
                            if (currentLap > kTotalLaps) {
                                state = GameState::FINISH;
                                break;
                            } else {
                                // Continuar circuito cerrado a la siguiente vuelta sin perder velocidad
                                car1.addDistance(-trackLength);
                                currentLapTime = 0.0f;
                                currentSectorTime = 0.0f;
                                sectorBannerTimer = 2.5f;
                                sectorDelta = (completedLapTimes.size() > 1) ? (lastLapTime - bestLapTime) : 0.0f;
                                currentSector = 1; // Volvemos a iniciar en Sector 1
                            }
                        }

                        prevTrackZ = car1.getTrackZ();

                    } else {
                        // MODO MULTIJUGADOR COOPERATIVO (2P CHECKPOINT ORIGINAL)
                        int curIdx1 = curSeg1.index;
                        if (curIdx1 >= nextCheckpointSegment1 && curIdx1 < nextCheckpointSegment1 + 8) {
                            checkpointsPassed1++;
                            nextCheckpointSegment1 += 60;
                            timeRemaining += 25.0f;
                            bonusBannerTimer1 = 2.0f;
                            sound.playCheckpoint();
                            input1.setRumble(0.4f, 0.7f, 200);
                            input1.setLED(34, 197, 94);
                        }
                    }
                }

                // Actualizar P2 (si está activo el modo 2P, 100% automático sin paletas ni botones)
                if (isSplitScreen) {
                    const RoadSegment& curSeg2 = road.findSegment(car2.getTrackZ());
                    car2.update(dt, in2, curSeg2.curve, false);

                    int curIdx2 = curSeg2.index;
                    if (curIdx2 >= nextCheckpointSegment2 && curIdx2 < nextCheckpointSegment2 + 8) {
                        checkpointsPassed2++;
                        nextCheckpointSegment2 += 60;
                        timeRemaining += 25.0f;
                        bonusBannerTimer2 = 2.0f;
                        sound.playCheckpoint();
                        input2.setRumble(0.4f, 0.7f, 200);
                        input2.setLED(34, 197, 94);
                    }
                }

                // COLISIÓN DIRECTA ENTRE P1 Y P2 (RUEDA A RUEDA)
                if (isSplitScreen) {
                    float dzP1P2 = car1.getTrackZ() - car2.getTrackZ();
                    float trackLen = road.getTrackLength();
                    while (dzP1P2 > trackLen * 0.5f) dzP1P2 -= trackLen;
                    while (dzP1P2 < -trackLen * 0.5f) dzP1P2 += trackLen;

                    float distZP1P2 = std::abs(dzP1P2);
                    float distXP1P2 = std::abs(car1.getX() - car2.getX());

                    if (distZP1P2 < 8.0f && distXP1P2 < 0.36f) {
                        // Impacto lateral o por alcance entre rivales
                        float bumpForce = (car1.getX() > car2.getX()) ? 0.22f : -0.22f;
                        car1.bumpX(bumpForce);
                        car2.bumpX(-bumpForce);

                        sound.playCrash();
                        input1.setRumble(0.8f, 0.5f, 300, true);
                        input2.setRumble(0.8f, 0.5f, 300, true);
                        input1.setLED(239, 68, 68);
                        input2.setLED(239, 68, 68);
                    }
                }

                // Comprobación de meta en 2P
                if (isSplitScreen && (car1.getTrackZ() >= road.getTrackLength() || car2.getTrackZ() >= road.getTrackLength())) {
                    state = GameState::FINISH;
                    sound.playCheckpoint();
                    winnerPlayer = (car1.getTrackZ() >= car2.getTrackZ()) ? 1 : 2;
                    break;
                }

                // Comprobación de tiempo agotado (Solo en 2P)
                if (isSplitScreen && timeRemaining <= 0.0f) {
                    state = GameState::GAME_OVER;
                    sound.playCrash();
                    break;
                }

                // Colisiones con tráfico
                {
                    float trackLength = road.getTrackLength();
                    static float collisionCooldown1 = 0.0f;
                    static float collisionCooldown2 = 0.0f;
                    if (collisionCooldown1 > 0.0f) collisionCooldown1 -= dt;
                    if (collisionCooldown2 > 0.0f) collisionCooldown2 -= dt;

                    for (auto& tCar : road.getTraffic()) {
                        // P1 vs Tráfico
                        float dz1 = tCar.z - car1.getTrackZ();
                        while (dz1 > trackLength * 0.5f) dz1 -= trackLength;
                        while (dz1 < -trackLength * 0.5f) dz1 += trackLength;

                        if (std::abs(dz1) < 9.0f && std::abs(tCar.x - car1.getX()) < 0.38f && collisionCooldown1 <= 0.0f) {
                            car1.hitObstacle();
                            sound.playCrash();
                            input1.setRumble(1.0f, 0.25f, 650, true);
                            input1.setLED(239, 68, 68);
                            tCar.speed = std::max(60.0f, tCar.speed + 30.0f);
                            tCar.z += 12.0f;
                            collisionCooldown1 = 0.8f;
                        }

                        // P2 vs Tráfico
                        if (isSplitScreen) {
                            float dz2 = tCar.z - car2.getTrackZ();
                            while (dz2 > trackLength * 0.5f) dz2 -= trackLength;
                            while (dz2 < -trackLength * 0.5f) dz2 += trackLength;

                            if (std::abs(dz2) < 9.0f && std::abs(tCar.x - car2.getX()) < 0.38f && collisionCooldown2 <= 0.0f) {
                                car2.hitObstacle();
                                sound.playCrash();
                                input2.setRumble(1.0f, 0.25f, 650, true);
                                input2.setLED(239, 68, 68);
                                tCar.speed = std::max(60.0f, tCar.speed + 30.0f);
                                tCar.z += 12.0f;
                                collisionCooldown2 = 0.8f;
                            }
                        }
                    }
                }

                // Actualizar Audio del Motor según el coche con mayor RPM
                {
                    float leadRpm = isSplitScreen ? std::max(car1.getRpm(), car2.getRpm()) : car1.getRpm();
                    float leadThrottle = isSplitScreen ? std::max(in1.throttle, in2.throttle) : in1.throttle;
                    sound.updateEngine(leadRpm, leadThrottle);
                    sound.playScreech(car1.isDrifting() || (isSplitScreen && car2.isDrifting()));
                }

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

                // Háptica para P2
                if (isSplitScreen) {
                    if (input2.isRumblePriorityActive()) {
                    } else if (car2.isOffRoad() && car2.getSpeed() > 20.0f) {
                        input2.setRumble(0.28f, 0.38f, 60);
                    } else if (car2.isDrifting()) {
                        input2.setRumble(0.20f, 0.30f, 60);
                    } else if (car2.getSpeed() > 10.0f) {
                        int gear = std::clamp(car2.getGear(), 1, 6);
                        float gearBase = (gear - 1) * 0.010f;
                        float rpmNorm = std::clamp((car2.getRpm() - 1000.0f) / 6000.0f, 0.0f, 1.0f);
                        float lowFreq = 0.020f + gearBase + (in2.throttle * 0.025f) + (rpmNorm * 0.020f);
                        float highFreq = 0.015f + (gearBase * 0.7f) + (car2.getSpeedRatio() * 0.035f);
                        input2.setRumble(lowFreq, highFreq, 45);
                    }

                    float rpm2 = car2.getRpm();
                    if (rpm2 < 4000.0f) input2.setLED(34, 197, 94);
                    else if (rpm2 < 5800.0f) input2.setLED(250, 204, 21);
                    else if (rpm2 < 6800.0f) input2.setLED(249, 115, 22);
                    else input2.setLED(239, 68, 68);
                }
                break;

            case GameState::FINISH:
            case GameState::GAME_OVER:
                sound.playScreech(false);
                if (in1.confirm || (isSplitScreen && in2.confirm)) {
                    car1.reset();
                    car2.reset();
                    if (isSplitScreen) {
                        car1.setInitialPosition(-0.35f, 0.0f);
                        car2.setInitialPosition( 0.35f, 0.0f);
                    }
                    raceTime = 0.0f;
                    timeRemaining = 45.0f;
                    checkpointsPassed1 = 0;
                    checkpointsPassed2 = 0;
                    nextCheckpointSegment1 = 60;
                    nextCheckpointSegment2 = 60;
                    winnerPlayer = 0;

                    // Reinicio variables Time Attack
                    currentLap = 1;
                    currentLapTime = 0.0f;
                    currentSectorTime = 0.0f;
                    lastLapTime = 0.0f;
                    prevTrackZ = 0.0f;
                    sectorBannerTimer = 0.0f;
                    completedLapTimes.clear();

                    state = GameState::RACING;
                } else if (in1.cancel || (isSplitScreen && in2.cancel)) {
                    running = false;
                }
                break;

            case GameState::PAUSE:
                sound.playScreech(false);
                if (in1.cancel || (isSplitScreen && in2.cancel)) {
                    state = GameState::RACING;
                }
                break;
        }

        // Renderizado del Juego
        if (isSplitScreen) {
            renderer.renderGame(road, car1, raceTime, &car2, {244, 63, 94, 255});
        } else {
            renderer.renderGame(road, car1, raceTime);
        }

        // Renderizado de Interfaces (UI)
        if (state == GameState::CALIBRATION) {
            ui.renderCalibration(in1, 1, isSplitScreen);
            if (isSplitScreen) {
                ui.renderCalibration(in2, 2, true);
            }
        } else if (state == GameState::RACING) {
            if (isSplitScreen) {
                float gap1 = car1.getTrackZ() - car2.getTrackZ();
                ui.renderHud(car1, timeRemaining, checkpointsPassed1, bonusBannerTimer1, true, 1, gap1);
                ui.renderHud(car2, timeRemaining, checkpointsPassed2, bonusBannerTimer2, true, 2, -gap1);
            } else {
                // HUD DE VUELTA RÁPIDA / TIME ATTACK CON MINI MAPA DE SECTORES
                ui.renderTimeAttackHud(car1, road, currentLap, kTotalLaps, currentLapTime, bestLapTime, lastLapTime, currentSector, sectorDelta, sectorBannerTimer);
            }
        } else if (state == GameState::FINISH) {
            if (isSplitScreen) {
                float topSpeed = std::max(topSpeedAchieved1, topSpeedAchieved2);
                int totalCk = std::max(checkpointsPassed1, checkpointsPassed2);
                ui.renderFinish(raceTime, topSpeed, totalCk, true, isSplitScreen, winnerPlayer);
            } else {
                // PANTALLA DE RESULTADOS DE VUELTA RÁPIDA
                ui.renderTimeAttackFinish(bestLapTime, raceTime, topSpeedAchieved1, completedLapTimes);
            }
        } else if (state == GameState::GAME_OVER) {
            ui.renderFinish(raceTime, topSpeedAchieved1, checkpointsPassed1, false, isSplitScreen, 0);
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
