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
        1280, 720,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
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
    InputManager input(config);
    input.init();

    SoundManager sound;
    sound.init();

    Car car;
    Road road;
    Renderer renderer(ren, fontTitle, fontHud, fontSmall);
    UserInterface ui(renderer, fontTitle, fontHud, fontSmall, config);

    GameState state = GameState::CALIBRATION;
    bool running = true;

    float raceTime = 0.0f;
    float timeRemaining = 45.0f; // 45 segundos para el primer checkpoint
    int checkpointsPassed = 0;
    int nextCheckpointSegment = 60;
    float bonusBannerTimer = 0.0f;
    float topSpeedAchieved = 0.0f;
    int pauseMenuIndex = 0;

    Uint32 lastTime = SDL_GetTicks();

    while (running) {
        Uint32 currentTime = SDL_GetTicks();
        float dt = (currentTime - lastTime) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f;
        lastTime = currentTime;

        input.clearMenuFlags();

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = false;
                break;
            }
            input.handleEvent(ev);

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

        input.update(dt);
        const InputState& in = input.getState();

        // Máquina de estados
        switch (state) {
            case GameState::CALIBRATION:
                if (in.confirm) {
                    input.calibrate();
                    state = GameState::RACING;
                } else if (in.cancel) {
                    running = false;
                }
                break;

            case GameState::RACING:
                if (in.pause) {
                    state = GameState::PAUSE;
                    pauseMenuIndex = 0;
                    break;
                }
                if (in.cancel) {
                    running = false; // Salir al menú
                    break;
                }

                raceTime += dt;
                timeRemaining -= dt;
                if (bonusBannerTimer > 0.0f) bonusBannerTimer -= dt;

                if (car.getSpeed() > topSpeedAchieved) topSpeedAchieved = car.getSpeed();

                // Curvatura del segmento actual
                {
                    const RoadSegment& curSeg = road.findSegment(car.getTrackZ());
                    car.update(dt, in, curSeg.curve);
                    road.update(dt);

                    // Cruce de Checkpoint
                    int curIdx = curSeg.index;
                    if (curIdx >= nextCheckpointSegment && curIdx < nextCheckpointSegment + 8) {
                        checkpointsPassed++;
                        nextCheckpointSegment += 60;
                        timeRemaining += 25.0f; // Bonificación de tiempo
                        bonusBannerTimer = 2.0f;
                        sound.playCheckpoint();
                        input.setRumble(0.4f, 0.7f, 200);
                        input.setLED(34, 197, 94); // Destello verde
                    }

                    // Comprobación de meta / circuito completo
                    if (car.getTrackZ() >= road.getTrackLength()) {
                        state = GameState::FINISH;
                        sound.playCheckpoint();
                        break;
                    }

                    // Comprobación de tiempo agotado
                    if (timeRemaining <= 0.0f) {
                        state = GameState::GAME_OVER;
                        sound.playCrash();
                        break;
                    }

                    // Colisión con tráfico
                    for (auto& tCar : road.getTraffic()) {
                        float distZ = std::abs(tCar.z - car.getTrackZ());
                        float distX = std::abs(tCar.x - car.getX());
                        if (distZ < 8.0f && distX < 0.45f) {
                            // Choque
                            car.hitObstacle();
                            sound.playCrash();
                            input.setRumble(0.9f, 1.0f, 250);
                            input.setLED(239, 68, 68); // Destello rojo choque
                            tCar.speed = car.getSpeed() * 0.8f;
                            break;
                        }
                    }

                    // Actualizar Audio del Motor y Chirridos
                    sound.updateEngine(car.getRpm(), in.throttle);
                    sound.playScreech(car.isDrifting());

                    // Vibración háptica en marcha y césped
                    if (car.isOffRoad() && car.getSpeed() > 20.0f) {
                        input.setRumble(0.35f, 0.55f, 50);
                    } else if (car.isDrifting()) {
                        input.setRumble(0.25f, 0.45f, 50);
                    }

                    // LED Tacómetro dinámico según RPM
                    float rpm = car.getRpm();
                    if (rpm < 4000.0f) input.setLED(34, 197, 94);       // Verde
                    else if (rpm < 5800.0f) input.setLED(250, 204, 21); // Amarillo
                    else if (rpm < 6800.0f) input.setLED(249, 115, 22); // Naranja
                    else input.setLED(239, 68, 68);                     // Rojo corte
                }
                break;

            case GameState::FINISH:
            case GameState::GAME_OVER:
                sound.playScreech(false);
                if (in.confirm) {
                    car.reset();
                    raceTime = 0.0f;
                    timeRemaining = 45.0f;
                    checkpointsPassed = 0;
                    nextCheckpointSegment = 60;
                    state = GameState::RACING;
                } else if (in.cancel) {
                    running = false;
                }
                break;

            case GameState::PAUSE:
                sound.playScreech(false);
                if (in.cancel) {
                    state = GameState::RACING;
                }
                break;
        }

        // Renderizado
        renderer.renderGame(road, car, raceTime);

        if (state == GameState::CALIBRATION) {
            ui.renderCalibration(in);
        } else if (state == GameState::RACING) {
            ui.renderHud(car, timeRemaining, checkpointsPassed, bonusBannerTimer);
        } else if (state == GameState::FINISH) {
            ui.renderFinish(raceTime, topSpeedAchieved, checkpointsPassed, true);
        } else if (state == GameState::GAME_OVER) {
            ui.renderFinish(raceTime, topSpeedAchieved, checkpointsPassed, false);
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
