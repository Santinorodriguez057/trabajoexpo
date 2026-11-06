#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <cmath>

#include "theme.h"
#include "input.h"
#include "entity.h"
#include "spawner.h"
#include "collision.h"
#include "score.h"
#include "renderer.h"

namespace {
    // Generador procedural de efectos de sonido sintetizados (WAV mono 16-bit)
    Mix_Chunk* createProceduralTone(int durationMs, int freqStart, int freqEnd, float volume = 0.5f, int type = 0) {
        int sampleRate = 44100;
        int numSamples = (sampleRate * durationMs) / 1000;
        int16_t* buffer = (int16_t*)std::malloc(numSamples * sizeof(int16_t));
        if (!buffer) return nullptr;

        for (int i = 0; i < numSamples; ++i) {
            float frac = static_cast<float>(i) / numSamples;
            float t = static_cast<float>(i) / sampleRate;
            float curFreq = freqStart + (freqEnd - freqStart) * frac;
            float env = std::exp(-frac * 6.0f);
            float sample = 0.0f;

            if (type == 0) {
                // Tajo de espada / Katana Slice (Ruido filtrado con silbido de alta frecuencia)
                float noise = ((std::rand() % 2000) / 1000.0f) - 1.0f;
                sample = std::sin(2.0f * 3.14159f * curFreq * t) * 0.4f + noise * 0.6f;
            } else if (type == 1) {
                // Corte perfecto / Hit metálico con armónicos
                sample = std::sin(2.0f * 3.14159f * curFreq * t) * 0.6f +
                         std::sin(2.0f * 3.14159f * curFreq * 2.2f * t) * 0.4f;
            } else if (type == 2) {
                // Explosión de mina PEM (Sub-grave y ruido estático)
                float noise = ((std::rand() % 2000) / 1000.0f) - 1.0f;
                float sub = std::sin(2.0f * 3.14159f * 65.0f * t);
                sample = noise * 0.7f + sub * 0.5f;
            }

            float val = std::clamp(sample * env * volume * 32760.0f, -32760.0f, 32760.0f);
            buffer[i] = static_cast<int16_t>(val);
        }

        Mix_Chunk* chunk = (Mix_Chunk*)std::malloc(sizeof(Mix_Chunk));
        if (!chunk) { std::free(buffer); return nullptr; }
        chunk->allocated = 1;
        chunk->abuf = (Uint8*)buffer;
        chunk->alen = numSamples * sizeof(int16_t);
        chunk->volume = static_cast<Uint8>(volume * 128);
        return chunk;
    }

    TTF_Font* loadSystemFont(int size) {
        std::vector<std::string> candidates = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"
        };
        for (const auto& path : candidates) {
            TTF_Font* f = TTF_OpenFont(path.c_str(), size);
            if (f) return f;
        }
        return nullptr;
    }
}

enum class GameState {
    MODE_SELECT, // Elegir Solo o 1v1
    PLAYING,
    GAME_OVER
};

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "Error al iniciar SDL: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        std::fprintf(stderr, "Error al iniciar TTF: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024);

    const int kWidth = 1920;
    const int kHeight = 1080;

    SDL_Window* window = SDL_CreateWindow("Ninja Slasher",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          kWidth, kHeight,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window) {
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_RenderSetLogicalSize(renderer, kWidth, kHeight);
    SDL_ShowCursor(SDL_DISABLE); // Ocultar cursor del mouse en todo el juego

    TTF_Font* fontTitle = loadSystemFont(54);
    TTF_Font* fontHud   = loadSystemFont(32);
    TTF_Font* fontSmall = loadSystemFont(22);

    Mix_Chunk* sfxSlice   = createProceduralTone(120, 800, 250, 0.6f, 0);
    Mix_Chunk* sfxPerfect = createProceduralTone(180, 520, 1040, 0.7f, 1);
    Mix_Chunk* sfxExplode = createProceduralTone(380, 180, 50, 0.85f, 2);

    // Sistema de entrada táctil para P1 y P2
    TouchInput touchP1(0, CyberTheme::cP1Cyan);
    TouchInput touchP2(1, CyberTheme::cP2Pink);

    // Detección automática unificada: mandos DualShock 4 prioritarios y Fight Sticks arcade
    auto autoAssignControllers = [&]() {
        // Cerrar mandos previos
        touchP1.setController(nullptr);
        touchP1.setArcadeStick(nullptr);
        touchP2.setController(nullptr);
        touchP2.setArcadeStick(nullptr);

        int totalJoys = SDL_NumJoysticks();
        std::vector<SDL_GameController*> ds4Controllers;
        std::vector<SDL_Joystick*> generalSticks;

        for (int i = 0; i < totalJoys; ++i) {
            if (SDL_IsGameController(i)) {
                SDL_GameController* gc = SDL_GameControllerOpen(i);
                if (gc) {
                    ds4Controllers.push_back(gc);
                }
            } else {
                SDL_Joystick* stick = SDL_JoystickOpen(i);
                if (stick) {
                    generalSticks.push_back(stick);
                }
            }
        }

        // Asignar mandos:
        // El Modo de a 2 (Versus) REQUIERE obligatoriamente DOS DUALSHOCK con Touchpad.
        if (ds4Controllers.size() >= 2) {
            touchP1.setController(ds4Controllers[0]);
            touchP2.setController(ds4Controllers[1]);
        } else if (ds4Controllers.size() == 1) {
            // Un solo DualShock: juega solo con Touchpad
            touchP1.setController(ds4Controllers[0]);
        } else if (!generalSticks.empty()) {
            // Sin DualShock: fallback de 1 jugador con Fight Stick
            touchP1.setArcadeStick(generalSticks[0]);
        }
    };

    autoAssignControllers();

    GameRenderer gameRen(renderer, fontTitle, fontHud, fontSmall);
    Spawner spawner(kWidth, kHeight);
    ScoreSystem scoreSys;

    std::vector<CyberEntity> entities;
    std::vector<CutHalf> halves;
    std::vector<SparkParticle> particles;

    GameState state = GameState::MODE_SELECT;
    bool isVersus = false;
    float matchTimer = 60.0f;
    float slowMoTimer = 0.0f;

    bool running = true;
    Uint64 lastPerfCount = SDL_GetPerformanceCounter();
    const double perfFreq = static_cast<double>(SDL_GetPerformanceFrequency());

    while (running) {
        Uint64 curPerfCount = SDL_GetPerformanceCounter();
        float dtReal = static_cast<float>(static_cast<double>(curPerfCount - lastPerfCount) / perfFreq);
        dtReal = std::clamp(dtReal, 0.0001f, 0.05f);
        lastPerfCount = curPerfCount;

        // Efecto cámara lenta cinematográfica en cortes "Perfecto"
        float dt = dtReal;
        if (slowMoTimer > 0.0f) {
            slowMoTimer -= dtReal;
            dt *= 0.35f;
        }

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = false;
                break;
            }

            // Detección automática en caliente de mandos conectados/desconectados
            if (ev.type == SDL_CONTROLLERDEVICEADDED || ev.type == SDL_CONTROLLERDEVICEREMOVED ||
                ev.type == SDL_JOYDEVICEADDED || ev.type == SDL_JOYDEVICEREMOVED) {
                autoAssignControllers();
            }

            // Tecla de salida rápida / volver al menú
            if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
            } else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
                if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_BACK || ev.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                    running = false;
                }
            }

            // Eventos táctiles
            touchP1.handleEvent(ev, kWidth, kHeight);
            touchP2.handleEvent(ev, kWidth, kHeight);
        }

        // --- ACTUALIZACIÓN SEGÚN ESTADO ---
        bool hasTwoDualshocks = (touchP1.getController() != nullptr && touchP2.getController() != nullptr);

        if (state == GameState::MODE_SELECT) {
            touchP1.update(dtReal, kWidth, kHeight);
            touchP2.update(dtReal, kWidth, kHeight);

            // Al tocar el touchpad o presionar [X] / [Cruz] inicia la partida
            bool startSolo = false;
            bool start1v1  = false;

            if (hasTwoDualshocks) {
                // Si hay 2 DUALSHOCK conectados, se habilita el modo 1v1
                if (touchP1.isFingerDown() && !touchP2.isFingerDown()) {
                    if (touchP1.getFingerX() < kWidth * 0.5f) startSolo = true;
                    else start1v1 = true;
                }
                if (touchP1.getController() && SDL_GameControllerGetButton(touchP1.getController(), SDL_CONTROLLER_BUTTON_A)) {
                    startSolo = true;
                }
                if (touchP2.getController() && SDL_GameControllerGetButton(touchP2.getController(), SDL_CONTROLLER_BUTTON_A)) {
                    start1v1 = true;
                }
                if (touchP2.isFingerDown()) {
                    start1v1 = true;
                }
            } else {
                // Modo Solo: con 1 DualShock o 1 Fight Stick
                if (touchP1.isFingerDown()) {
                    startSolo = true;
                }
                if (touchP1.getController() && SDL_GameControllerGetButton(touchP1.getController(), SDL_CONTROLLER_BUTTON_A)) {
                    startSolo = true;
                }
                if (touchP1.getArcadeStick()) {
                    int nb = SDL_JoystickNumButtons(touchP1.getArcadeStick());
                    for (int b = 0; b < nb && b < 4; ++b) {
                        if (SDL_JoystickGetButton(touchP1.getArcadeStick(), b)) {
                            startSolo = true;
                            break;
                        }
                    }
                }
            }

            if (startSolo || start1v1) {
                isVersus = start1v1 && hasTwoDualshocks;
                state = GameState::PLAYING;
                matchTimer = 60.0f;
                scoreSys.reset();
                entities.clear();
                halves.clear();
                particles.clear();
                spawner.reset();
            }

        } else if (state == GameState::PLAYING) {
            if (isVersus) {
                matchTimer -= dtReal;
                if (matchTimer <= 0.0f) {
                    state = GameState::GAME_OVER;
                }
            } else {
                // Modo Solo: dura hasta que el jugador se queda sin vidas (por bomba o por dejar caer objetos)
                if (scoreSys.isGameOver()) {
                    state = GameState::GAME_OVER;
                }
            }

            touchP1.update(dtReal, kWidth, kHeight);
            if (isVersus) {
                touchP2.update(dtReal, kWidth, kHeight);
            }

            spawner.update(dt, entities);

            for (auto& e : entities) {
                e.update(dt);
                // En Modo Solo: dejar caer un objeto puntuable (que no sea mina PEM) quita 1 vida
                if (!isVersus && e.isDropped() && e.getType() != EntityType::EMP_MINE) {
                    scoreSys.registerDropMiss(e.getX(), kHeight - 80.0f);
                    Mix_PlayChannel(-1, sfxExplode, 0);
                    touchP1.triggerHaptic(0.8f, 0.5f, 250);
                    touchP1.setLED(239, 68, 68);
                }
            }

            // Procesar cortes de ambos jugadores
            std::vector<SlashSegment> slashes = touchP1.getActiveSlashes();
            if (isVersus) {
                const auto& p2Slashes = touchP2.getActiveSlashes();
                slashes.insert(slashes.end(), p2Slashes.begin(), p2Slashes.end());
            }

            for (const auto& slash : slashes) {
                for (auto& e : entities) {
                    if (!e.isAlive()) continue;

                    CollisionResult res = CollisionSystem::evaluateSlash(slash, e);
                    if (res.hit) {
                        e.kill();
                        e.spawnHalves(res.cutAngle, halves);

                        // Generar chispas neobrutalistas
                        for (int p = 0; p < 14; ++p) {
                            SparkParticle sp;
                            sp.x = e.getX();
                            sp.y = e.getY();
                            float a = (std::rand() % 628) / 100.0f;
                            float s = 140.0f + (std::rand() % 320);
                            sp.vx = std::cos(a) * s;
                            sp.vy = std::sin(a) * s;
                            sp.size = 6.0f + (std::rand() % 6);
                            sp.life = 0.45f;
                            sp.color = (slash.playerIndex == 0) ? CyberTheme::cP1Cyan : CyberTheme::cP2Pink;
                            particles.push_back(sp);
                        }

                        TouchInput& activeTouch = (slash.playerIndex == 0) ? touchP1 : touchP2;

                        if (res.isMine) {
                            scoreSys.registerMineHit(slash.playerIndex, e.getX(), e.getY());
                            Mix_PlayChannel(-1, sfxExplode, 0);
                            activeTouch.triggerHaptic(0.9f, 0.9f, 350);
                            activeTouch.setLED(239, 68, 68); // Rojo
                        } else {
                            bool isPerf = (res.rating == CutRating::PERFECT);
                            scoreSys.addCut(slash.playerIndex, res.points, isPerf, e.getX(), e.getY());

                            if (isPerf) {
                                slowMoTimer = 0.16f; // Breve pausa cinematográfica
                                Mix_PlayChannel(-1, sfxPerfect, 0);
                                activeTouch.triggerHaptic(0.5f, 0.8f, 180);
                                activeTouch.setLED(34, 197, 94); // Verde
                            } else {
                                Mix_PlayChannel(-1, sfxSlice, 0);
                                activeTouch.triggerHaptic(0.3f, 0.4f, 100);
                                activeTouch.setLED(250, 204, 21); // Amarillo
                            }
                        }
                    }
                }
            }

            // Limpiar entidades muertas
            entities.erase(std::remove_if(entities.begin(), entities.end(),
                                          [](const CyberEntity& e) { return !e.isAlive(); }),
                           entities.end());

            // Actualizar mitades cortadas
            for (auto& h : halves) {
                h.vy += 980.0f * dt;
                h.x += h.vx * dt;
                h.y += h.vy * dt;
                h.angle += h.vRot * dt;
                h.life -= dt * 0.9f;
            }
            halves.erase(std::remove_if(halves.begin(), halves.end(),
                                        [](const CutHalf& h) { return h.life <= 0.0f || h.y > 1250.0f; }),
                         halves.end());

            // Actualizar partículas
            for (auto& p : particles) {
                p.x += p.vx * dt;
                p.y += p.vy * dt;
                p.life -= dt * 2.2f;
            }
            particles.erase(std::remove_if(particles.begin(), particles.end(),
                                           [](const SparkParticle& p) { return p.life <= 0.0f; }),
                            particles.end());

            scoreSys.update(dt);

        } else if (state == GameState::GAME_OVER) {
            // Reiniciar EXCLUSIVAMENTE con el botón [X] (Cruz / Button A) del mando o botón 0 del arcade stick
            bool btnRestart = false;
            if (touchP1.getController() && SDL_GameControllerGetButton(touchP1.getController(), SDL_CONTROLLER_BUTTON_A)) {
                btnRestart = true;
            }
            if (isVersus && touchP2.getController() && SDL_GameControllerGetButton(touchP2.getController(), SDL_CONTROLLER_BUTTON_A)) {
                btnRestart = true;
            }
            if (touchP1.getArcadeStick() && SDL_JoystickGetButton(touchP1.getArcadeStick(), 0)) {
                btnRestart = true;
            }

            if (btnRestart) {
                state = GameState::MODE_SELECT;
            }
        }

        // --- RENDERIZADO VISUAL NEOBRUTALISTA ---
        gameRen.renderBackground(kWidth, kHeight, static_cast<float>(curPerfCount) / perfFreq);
        gameRen.renderEntities(entities);
        gameRen.renderHalves(halves);
        gameRen.renderParticles(particles);

        // Estelas de katana
        gameRen.renderTrails(touchP1.getTrail(), CyberTheme::cP1Trail);
        if (isVersus) {
            gameRen.renderTrails(touchP2.getTrail(), CyberTheme::cP2Trail);
        }

        gameRen.renderFloatingScores(scoreSys.getFloatingScores());

        // Interfaz según estado
        if (state == GameState::MODE_SELECT) {
            // Cartel de Selección de Modo Neobrutalista
            int boxW = 860;
            int boxH = 460;
            int bx = kWidth / 2 - boxW / 2;
            int by = kHeight / 2 - boxH / 2;
            gameRen.drawNeobrutalRect(bx, by, boxW, boxH, CyberTheme::cWhite, 5, 8);
            gameRen.drawText(fontTitle, "NINJA SLASHER", kWidth / 2, by + 40, CyberTheme::cBlack, true);
            
            std::string statusText;
            if (hasTwoDualshocks) {
                statusText = "2 DUALSHOCK DETECTADOS - TOCA PARA ELEGIR MODO";
            } else if (touchP1.getController()) {
                statusText = "1 DUALSHOCK DETECTADO - MODO SOLO (CONECTA 2 DUALSHOCK PARA 1V1)";
            } else if (touchP1.getArcadeStick()) {
                statusText = "FIGHT STICK DETECTADO - MODO SOLO";
            } else {
                statusText = "CONECTA UN MANDO DUALSHOCK 4";
            }
            gameRen.drawText(fontSmall, statusText, kWidth / 2, by + 115, hasTwoDualshocks ? CyberTheme::cGreen : CyberTheme::cBlack, true);

            if (hasTwoDualshocks) {
                // Opción 1: MODO SOLO (3 VIDAS)
                gameRen.drawNeobrutalRect(bx + 60, by + 175, 330, 210, CyberTheme::cP1Cyan, 4, 6);
                gameRen.drawText(fontHud, "MODO SOLO", bx + 225, by + 205, CyberTheme::cBlack, true);
                gameRen.drawText(fontSmall, "3 VIDAS", bx + 225, by + 255, CyberTheme::cBlack, true);
                gameRen.drawText(fontSmall, "TOCA LA IZQUIERDA", bx + 225, by + 315, CyberTheme::cBlack, true);

                // Opción 2: MODO 1V1 VERSUS (60 SEGUNDOS)
                gameRen.drawNeobrutalRect(bx + 470, by + 175, 330, 210, CyberTheme::cP2Pink, 4, 6);
                gameRen.drawText(fontHud, "1V1 VERSUS", bx + 635, by + 205, CyberTheme::cBlack, true);
                gameRen.drawText(fontSmall, "DUELO 60s", bx + 635, by + 255, CyberTheme::cBlack, true);
                gameRen.drawText(fontSmall, "TOCA LA DERECHA", bx + 635, by + 315, CyberTheme::cBlack, true);
            } else {
                // Panel Único: Modo Solo Listo para Iniciar
                gameRen.drawNeobrutalRect(bx + 140, by + 175, 580, 210, CyberTheme::cP1Cyan, 4, 6);
                gameRen.drawText(fontHud, "MODO SOLO (1 JUGADOR)", kWidth / 2, by + 215, CyberTheme::cBlack, true);
                gameRen.drawText(fontSmall, "DESLIZA POR EL TOUCHPAD O PALANCA PARA COMENZAR", kWidth / 2, by + 275, CyberTheme::cBlack, true);
                gameRen.drawText(fontSmall, "3 VIDAS DE BLINDAJE", kWidth / 2, by + 325, CyberTheme::cBlack, true);
            }

        } else if (state == GameState::PLAYING) {
            if (isVersus) {
                gameRen.renderHud1v1(scoreSys.getScore(0), scoreSys.getCombo(0),
                                     scoreSys.getScore(1), scoreSys.getCombo(1),
                                     matchTimer);
            } else {
                gameRen.renderHudSolo(scoreSys.getScore(0), scoreSys.getCombo(0),
                                      scoreSys.getLives());
            }
        } else if (state == GameState::GAME_OVER) {
            int boxW = 760;
            int boxH = 400;
            int bx = kWidth / 2 - boxW / 2;
            int by = kHeight / 2 - boxH / 2;
            gameRen.drawNeobrutalRect(bx, by, boxW, boxH, CyberTheme::cYellow, 5, 8);
            gameRen.drawText(fontTitle, "FIN DE LA PARTIDA", kWidth / 2, by + 45, CyberTheme::cBlack, true);

            if (isVersus) {
                int s1 = scoreSys.getScore(0);
                int s2 = scoreSys.getScore(1);
                std::string winner;
                if (s1 > s2) winner = "¡GANA JUGADOR 1 (CIAN)!";
                else if (s2 > s1) winner = "¡GANA JUGADOR 2 (ROSA)!";
                else winner = "¡EMPATE TÉCNICO!";
                gameRen.drawText(fontHud, winner, kWidth / 2, by + 140, CyberTheme::cBlack, true);
                gameRen.drawText(fontSmall, "P1: " + std::to_string(s1) + " PTS   |   P2: " + std::to_string(s2) + " PTS", kWidth / 2, by + 210, CyberTheme::cBlack, true);
            } else {
                gameRen.drawText(fontHud, "PUNTAJE FINAL: " + std::to_string(scoreSys.getScore(0)), kWidth / 2, by + 160, CyberTheme::cBlack, true);
            }

            gameRen.drawText(fontSmall, "PRESIONA [X] PARA VOLVER A JUGAR", kWidth / 2, by + 310, CyberTheme::cBlack, true);
        }

        SDL_RenderPresent(renderer);
    }

    // Limpieza de recursos
    if (sfxSlice) Mix_FreeChunk(sfxSlice);
    if (sfxPerfect) Mix_FreeChunk(sfxPerfect);
    if (sfxExplode) Mix_FreeChunk(sfxExplode);

    if (fontTitle) TTF_CloseFont(fontTitle);
    if (fontHud) TTF_CloseFont(fontHud);
    if (fontSmall) TTF_CloseFont(fontSmall);

    Mix_CloseAudio();
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
