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
            } else if (type == 3) {
                // Pulso limpio de cuenta regresiva / UI
                sample = std::sin(2.0f * 3.14159f * curFreq * t) * 0.75f +
                         std::sin(2.0f * 3.14159f * curFreq * 1.5f * t) * 0.25f;
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

    char computeRank(int score) {
        if (score >= 4000) return 'S';
        if (score >= 2500) return 'A';
        if (score >= 1200) return 'B';
        if (score >= 500)  return 'C';
        return 'D';
    }
}

enum class GameState {
    MODE_SELECT, // Elegir Solo o 1v1 (con objetos de práctica en el fondo)
    COUNTDOWN,   // 3 - 2 - 1 - ¡YA!
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

    TTF_Font* fontGiant    = loadSystemFont(110);
    TTF_Font* fontTitleBig = loadSystemFont(78);
    TTF_Font* fontTitle    = loadSystemFont(50);
    TTF_Font* fontHud      = loadSystemFont(32);
    TTF_Font* fontSmall    = loadSystemFont(22);

    Mix_Chunk* sfxSlice   = createProceduralTone(120, 800, 250, 0.6f, 0);
    Mix_Chunk* sfxPerfect = createProceduralTone(180, 520, 1040, 0.75f, 1);
    Mix_Chunk* sfxExplode = createProceduralTone(380, 180, 50, 0.85f, 2);
    Mix_Chunk* sfxBeep    = createProceduralTone(110, 660, 660, 0.55f, 3);
    Mix_Chunk* sfxGo      = createProceduralTone(220, 880, 1320, 0.7f, 3);

    // Sistema de entrada táctil para P1 y P2
    TouchInput touchP1(0, CyberTheme::cP1Cyan);
    TouchInput touchP2(1, CyberTheme::cP2Pink);

    // Detección automática unificada: mandos DualShock 4 prioritarios y Fight Sticks arcade
    auto autoAssignControllers = [&]() {
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

        // El Modo de a 2 (Versus) REQUIERE obligatoriamente DOS DUALSHOCK con Touchpad.
        if (ds4Controllers.size() >= 2) {
            touchP1.setController(ds4Controllers[0]);
            touchP2.setController(ds4Controllers[1]);
        } else if (ds4Controllers.size() == 1) {
            touchP1.setController(ds4Controllers[0]);
        } else if (!generalSticks.empty()) {
            touchP1.setArcadeStick(generalSticks[0]);
        }
    };

    autoAssignControllers();

    GameRenderer gameRen(renderer, fontGiant, fontTitleBig, fontTitle, fontHud, fontSmall);
    Spawner spawner(kWidth, kHeight);
    ScoreSystem scoreSys;

    std::vector<CyberEntity> entities;
    std::vector<CutHalf> halves;
    std::vector<SparkParticle> particles;
    std::vector<ShockRing> rings;
    std::vector<SlashFlash> flashes;

    GameState state = GameState::MODE_SELECT;
    bool isVersus = false;
    int selectedMode = 0; // 0 = Solo, 1 = 1v1 Versus
    float globalTime = 0.0f;
    float menuAnim = 0.0f;
    float countdownTimer = 0.0f;
    int lastCountdownBeep = 4;
    float matchTimer = 60.0f;
    float slowMoTimer = 0.0f;
    float hitStopTimer = 0.0f;
    float shakeTimer = 0.0f;
    float shakeMag = 0.0f;
    float gameOverAnim = 0.0f;

    // Estadísticas y contadores animados (Game-Feel)
    float displayScoreF[2] = {0.0f, 0.0f};
    float scorePop[2] = {0.0f, 0.0f};
    float lifeShake = 0.0f;
    int cutsCount[2] = {0, 0};
    int perfectsCount[2] = {0, 0};
    int maxCombo[2] = {0, 0};
    int bestScore = 0;
    bool newRecordAchieved = false;
    bool prevBtnRestart = false;
    float menuDemoTimer = 0.8f;

    auto triggerShake = [&](float mag, float dur) {
        if (mag > shakeMag || shakeTimer <= 0.0f) {
            shakeMag = mag;
        }
        shakeTimer = std::max(shakeTimer, dur);
    };

    auto spawnCutEffects = [&](const CyberEntity& e, const SlashSegment& slash, const CollisionResult& res) {
        SDL_Color pCol = (slash.playerIndex == 0) ? CyberTheme::cP1Cyan : CyberTheme::cP2Pink;
        bool isPerf = (res.rating == CutRating::PERFECT);

        // Línea de destello de katana que atraviesa el objeto
        SlashFlash sf;
        sf.x = e.getX();
        sf.y = e.getY();
        sf.angle = res.cutAngle;
        sf.length = res.isMine ? 290.0f : (isPerf ? 260.0f : 200.0f);
        sf.life = 1.0f;
        sf.color = res.isMine ? CyberTheme::cRed : (isPerf ? CyberTheme::cYellow : pCol);
        flashes.push_back(sf);

        // Onda expansiva neobrutalista
        ShockRing sr;
        sr.x = e.getX();
        sr.y = e.getY();
        sr.radius = e.getRadius() * 0.55f;
        sr.maxRadius = res.isMine ? 220.0f : (isPerf ? 165.0f : 120.0f);
        sr.thickness = res.isMine ? 16.0f : 12.0f;
        sr.life = 1.0f;
        sr.color = res.isMine ? CyberTheme::cRed : (isPerf ? CyberTheme::cGreen : pCol);
        rings.push_back(sr);

        // Chispas geométricas rotatorias con borde negro
        int numSparks = res.isMine ? 26 : (isPerf ? 20 : 14);
        for (int p = 0; p < numSparks; ++p) {
            SparkParticle sp;
            sp.x = e.getX();
            sp.y = e.getY();
            float a = (std::rand() % 628) / 100.0f;
            float s = 180.0f + (std::rand() % 420);
            sp.vx = std::cos(a) * s;
            sp.vy = std::sin(a) * s - 80.0f;
            sp.size = 7.0f + (std::rand() % 8);
            sp.life = 0.55f + (std::rand() % 35) / 100.0f;
            sp.rot = static_cast<float>(std::rand() % 360);
            sp.vrot = -360.0f + static_cast<float>(std::rand() % 720);
            if (res.isMine) {
                sp.color = (p % 2 == 0) ? CyberTheme::cRed : CyberTheme::cYellow;
            } else if (isPerf && (p % 3 == 0)) {
                sp.color = CyberTheme::cYellow;
            } else {
                sp.color = pCol;
            }
            particles.push_back(sp);
        }
    };

    auto startCountdown = [&](bool versusMode) {
        isVersus = versusMode;
        state = GameState::COUNTDOWN;
        countdownTimer = 3.2f;
        lastCountdownBeep = 4;
        matchTimer = 60.0f;
        scoreSys.reset();
        entities.clear();
        halves.clear();
        particles.clear();
        rings.clear();
        flashes.clear();
        spawner.reset();
        displayScoreF[0] = displayScoreF[1] = 0.0f;
        scorePop[0] = scorePop[1] = 0.0f;
        lifeShake = 0.0f;
        cutsCount[0] = cutsCount[1] = 0;
        perfectsCount[0] = perfectsCount[1] = 0;
        maxCombo[0] = maxCombo[1] = 0;
        newRecordAchieved = false;
    };

    bool running = true;
    Uint64 lastPerfCount = SDL_GetPerformanceCounter();
    const double perfFreq = static_cast<double>(SDL_GetPerformanceFrequency());

    while (running) {
        Uint64 curPerfCount = SDL_GetPerformanceCounter();
        float dtReal = static_cast<float>(static_cast<double>(curPerfCount - lastPerfCount) / perfFreq);
        dtReal = std::clamp(dtReal, 0.0001f, 0.05f);
        lastPerfCount = curPerfCount;

        globalTime += dtReal;

        // Hit-stop (micro-pausa de impacto) y cámara lenta cinematográfica
        float dt = dtReal;
        if (hitStopTimer > 0.0f) {
            hitStopTimer -= dtReal;
            dt = 0.0f;
        } else if (slowMoTimer > 0.0f) {
            slowMoTimer -= dtReal;
            dt *= 0.35f;
        }

        if (shakeTimer > 0.0f) {
            shakeTimer = std::max(0.0f, shakeTimer - dtReal);
        }
        if (lifeShake > 0.0f) {
            lifeShake = std::max(0.0f, lifeShake - dtReal * 2.2f);
        }
        for (int i = 0; i < 2; ++i) {
            if (scorePop[i] > 0.0f) {
                scorePop[i] = std::max(0.0f, scorePop[i] - dtReal * 3.2f);
            }
        }

        bool hasTwoDualshocks = (touchP1.getController() != nullptr && touchP2.getController() != nullptr);
        if (!hasTwoDualshocks) {
            selectedMode = 0;
        }

        bool menuConfirmPressed = false;

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
                hasTwoDualshocks = (touchP1.getController() != nullptr && touchP2.getController() != nullptr);
            }

            // Tecla de salida rápida / navegación en menú
            if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (state == GameState::MODE_SELECT) {
                    if (ev.key.keysym.sym == SDLK_LEFT) selectedMode = 0;
                    if (ev.key.keysym.sym == SDLK_RIGHT && hasTwoDualshocks) selectedMode = 1;
                    if (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_SPACE) {
                        menuConfirmPressed = true;
                    }
                }
            } else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
                if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_BACK || ev.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                    running = false;
                } else if (state == GameState::MODE_SELECT) {
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
                        selectedMode = 0;
                        Mix_PlayChannel(-1, sfxBeep, 0);
                    } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT && hasTwoDualshocks) {
                        selectedMode = 1;
                        Mix_PlayChannel(-1, sfxBeep, 0);
                    } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                        menuConfirmPressed = true;
                    }
                }
            } else if (ev.type == SDL_JOYBUTTONDOWN && state == GameState::MODE_SELECT) {
                if (ev.jbutton.button == 0) {
                    menuConfirmPressed = true;
                }
            }

            // Eventos táctiles
            touchP1.handleEvent(ev, kWidth, kHeight);
            touchP2.handleEvent(ev, kWidth, kHeight);
        }

        // --- ACTUALIZACIÓN SEGÚN ESTADO ---
        if (state == GameState::MODE_SELECT) {
            menuAnim = std::min(1.0f, menuAnim + dtReal * 2.2f);
            touchP1.update(dtReal, kWidth, kHeight);
            touchP2.update(dtReal, kWidth, kHeight);

            // Lanzar algunos objetos de exhibición en el fondo para que el jugador pueda probar el touchpad
            menuDemoTimer -= dtReal;
            if (menuDemoTimer <= 0.0f && entities.size() < 3) {
                menuDemoTimer = 1.35f;
                EntityType demoTypes[] = {EntityType::ENERGY_CORE, EntityType::CYBER_DRONE, EntityType::MECHA_SHURIKEN};
                EntityType dtType = demoTypes[std::rand() % 3];
                float sx = 260.0f + (std::rand() % (kWidth - 520));
                float vx = (sx < kWidth * 0.5f) ? (80.0f + std::rand() % 140) : -(80.0f + std::rand() % 140);
                float vy = -(980.0f + (std::rand() % 180));
                float ideal = ((std::rand() % 6) - 2) * (3.14159f / 4.0f);
                entities.emplace_back(dtType, sx, kHeight + 50.0f, vx, vy, ideal);
            }

            for (auto& e : entities) e.update(dtReal);

            // Permitir cortar los objetos de demostración en el menú y elegir tarjeta con el touchpad
            std::vector<SlashSegment> demoSlashes = touchP1.getActiveSlashes();
            const auto& p2Demo = touchP2.getActiveSlashes();
            demoSlashes.insert(demoSlashes.end(), p2Demo.begin(), p2Demo.end());

            if (hasTwoDualshocks && touchP1.isFingerDown()) {
                selectedMode = (touchP1.getFingerX() < kWidth * 0.5f) ? 0 : 1;
            } else if (hasTwoDualshocks && touchP2.isFingerDown()) {
                selectedMode = 1;
            }

            for (const auto& slash : demoSlashes) {
                for (auto& e : entities) {
                    if (!e.isAlive()) continue;
                    CollisionResult res = CollisionSystem::evaluateSlash(slash, e);
                    if (res.hit) {
                        e.kill();
                        e.spawnHalves(res.cutAngle, halves);
                        spawnCutEffects(e, slash, res);
                        Mix_PlayChannel(-1, (res.rating == CutRating::PERFECT) ? sfxPerfect : sfxSlice, 0);
                        triggerShake(8.0f, 0.12f);
                    }
                }
            }
            entities.erase(std::remove_if(entities.begin(), entities.end(),
                                          [](const CyberEntity& e) { return !e.isAlive(); }),
                           entities.end());

            if (menuConfirmPressed) {
                Mix_PlayChannel(-1, sfxGo, 0);
                startCountdown(selectedMode == 1 && hasTwoDualshocks);
            }

        } else if (state == GameState::COUNTDOWN) {
            touchP1.update(dtReal, kWidth, kHeight);
            if (isVersus) touchP2.update(dtReal, kWidth, kHeight);

            countdownTimer -= dtReal;
            int curNum = static_cast<int>(std::ceil(countdownTimer - 0.35f));
            if (curNum < lastCountdownBeep && curNum >= 0) {
                lastCountdownBeep = curNum;
                Mix_PlayChannel(-1, (curNum > 0) ? sfxBeep : sfxGo, 0);
                if (curNum == 0) triggerShake(10.0f, 0.15f);
            }
            if (countdownTimer <= 0.0f) {
                state = GameState::PLAYING;
            }

        } else if (state == GameState::PLAYING) {
            if (isVersus) {
                matchTimer -= dtReal;
                if (matchTimer <= 0.0f) {
                    matchTimer = 0.0f;
                    state = GameState::GAME_OVER;
                    gameOverAnim = 0.0f;
                    displayScoreF[0] = displayScoreF[1] = 0.0f;
                    prevBtnRestart = true;
                }
            } else {
                // Modo Solo: dura hasta que el jugador pierde las 3 vidas (por mina o dejar caer objeto)
                if (scoreSys.isGameOver()) {
                    state = GameState::GAME_OVER;
                    gameOverAnim = 0.0f;
                    displayScoreF[0] = 0.0f;
                    prevBtnRestart = true;
                    if (scoreSys.getScore(0) > bestScore) {
                        bestScore = scoreSys.getScore(0);
                        newRecordAchieved = true;
                    }
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
                    lifeShake = 1.0f;
                    triggerShake(20.0f, 0.25f);
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
                        spawnCutEffects(e, slash, res);

                        int pIdx = slash.playerIndex;
                        TouchInput& activeTouch = (pIdx == 0) ? touchP1 : touchP2;

                        if (res.isMine) {
                            scoreSys.registerMineHit(pIdx, e.getX(), e.getY());
                            lifeShake = 1.0f;
                            hitStopTimer = 0.055f;
                            triggerShake(28.0f, 0.32f);
                            Mix_PlayChannel(-1, sfxExplode, 0);
                            activeTouch.triggerHaptic(0.9f, 0.9f, 350);
                            activeTouch.setLED(239, 68, 68);
                        } else {
                            bool isPerf = (res.rating == CutRating::PERFECT);
                            scoreSys.addCut(pIdx, res.points, isPerf, e.getX(), e.getY());
                            cutsCount[pIdx]++;
                            if (isPerf) perfectsCount[pIdx]++;
                            maxCombo[pIdx] = std::max(maxCombo[pIdx], scoreSys.getCombo(pIdx));
                            scorePop[pIdx] = 1.0f;

                            if (!isVersus && scoreSys.getScore(0) > bestScore) {
                                bestScore = scoreSys.getScore(0);
                                newRecordAchieved = true;
                            }

                            if (isPerf) {
                                hitStopTimer = 0.04f;
                                slowMoTimer = 0.16f;
                                triggerShake(16.0f, 0.18f);
                                Mix_PlayChannel(-1, sfxPerfect, 0);
                                activeTouch.triggerHaptic(0.5f, 0.8f, 180);
                                activeTouch.setLED(34, 197, 94);
                            } else {
                                triggerShake(9.0f, 0.12f);
                                Mix_PlayChannel(-1, sfxSlice, 0);
                                activeTouch.triggerHaptic(0.3f, 0.4f, 100);
                                activeTouch.setLED(250, 204, 21);
                            }
                        }
                    }
                }
            }

            // Limpiar entidades muertas
            entities.erase(std::remove_if(entities.begin(), entities.end(),
                                          [](const CyberEntity& e) { return !e.isAlive(); }),
                           entities.end());

            scoreSys.update(dt);

            // Animar puntaje progresivamente en el HUD
            for (int i = 0; i < 2; ++i) {
                float target = static_cast<float>(scoreSys.getScore(i));
                displayScoreF[i] += (target - displayScoreF[i]) * std::min(1.0f, dtReal * 14.0f);
                if (std::fabs(target - displayScoreF[i]) < 1.5f) displayScoreF[i] = target;
            }

        } else if (state == GameState::GAME_OVER) {
            gameOverAnim += dtReal;
            touchP1.update(dtReal, kWidth, kHeight);
            if (isVersus) touchP2.update(dtReal, kWidth, kHeight);

            for (int i = 0; i < 2; ++i) {
                float target = static_cast<float>(scoreSys.getScore(i));
                displayScoreF[i] += (target - displayScoreF[i]) * std::min(1.0f, dtReal * 8.0f);
                if (std::fabs(target - displayScoreF[i]) < 1.5f) displayScoreF[i] = target;
            }

            // Reiniciar EXCLUSIVAMENTE con el botón [X] (Cruz / Button A) del mando o botón 0 del arcade stick
            bool btnDown = false;
            if (touchP1.getController() && SDL_GameControllerGetButton(touchP1.getController(), SDL_CONTROLLER_BUTTON_A)) {
                btnDown = true;
            }
            if (isVersus && touchP2.getController() && SDL_GameControllerGetButton(touchP2.getController(), SDL_CONTROLLER_BUTTON_A)) {
                btnDown = true;
            }
            if (touchP1.getArcadeStick() && SDL_JoystickGetButton(touchP1.getArcadeStick(), 0)) {
                btnDown = true;
            }

            if (btnDown && !prevBtnRestart && gameOverAnim > 0.55f) {
                Mix_PlayChannel(-1, sfxBeep, 0);
                state = GameState::MODE_SELECT;
                menuAnim = 0.0f;
                entities.clear();
                halves.clear();
                particles.clear();
                rings.clear();
                flashes.clear();
            }
            prevBtnRestart = btnDown;
        }

        // Actualizar mitades cortadas, chispas, anillos y destellos (activos en todo momento)
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

        for (auto& p : particles) {
            p.vy += 520.0f * dt;
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.rot += p.vrot * dt;
            p.life -= dt * 1.9f;
        }
        particles.erase(std::remove_if(particles.begin(), particles.end(),
                                       [](const SparkParticle& p) { return p.life <= 0.0f; }),
                        particles.end());

        for (auto& r : rings) {
            r.radius += (r.maxRadius - r.radius) * std::min(1.0f, dtReal * 14.0f);
            r.life -= dtReal * 3.8f;
        }
        rings.erase(std::remove_if(rings.begin(), rings.end(),
                                   [](const ShockRing& r) { return r.life <= 0.0f; }),
                    rings.end());

        for (auto& f : flashes) {
            f.life -= dtReal * 5.5f;
        }
        flashes.erase(std::remove_if(flashes.begin(), flashes.end(),
                                     [](const SlashFlash& f) { return f.life <= 0.0f; }),
                      flashes.end());

        // --- RENDERIZADO VISUAL NEOBRUTALISTA ---
        float shakeX = 0.0f, shakeY = 0.0f;
        if (shakeTimer > 0.0f) {
            float curMag = shakeMag * std::min(1.0f, shakeTimer * 6.0f);
            shakeX = ((std::rand() % 200) / 100.0f - 1.0f) * curMag;
            shakeY = ((std::rand() % 200) / 100.0f - 1.0f) * curMag;
        }

        int maxCurCombo = std::max(scoreSys.getCombo(0), scoreSys.getCombo(1));
        int accentLevel = (maxCurCombo >= 8) ? 2 : ((maxCurCombo >= 4) ? 1 : 0);
        bool inDanger = (state == GameState::PLAYING) &&
                        ((!isVersus && scoreSys.getLives() == 1) || (isVersus && matchTimer <= 10.0f));

        gameRen.beginFrame();
        gameRen.beginWorld();

        gameRen.renderBackground(globalTime, accentLevel, inDanger);
        gameRen.renderRings(rings);
        gameRen.renderEntities(entities, globalTime);
        gameRen.renderHalves(halves);
        gameRen.renderParticles(particles);
        gameRen.renderSlashFlashes(flashes);

        // Estelas de katana + indicador del dedo en el touchpad
        gameRen.renderTrails(touchP1.getTrail(), CyberTheme::cP1Trail,
                             touchP1.isFingerDown(), touchP1.getFingerX(), touchP1.getFingerY());
        if (isVersus || (state == GameState::MODE_SELECT && hasTwoDualshocks)) {
            gameRen.renderTrails(touchP2.getTrail(), CyberTheme::cP2Trail,
                                 touchP2.isFingerDown(), touchP2.getFingerX(), touchP2.getFingerY());
        }

        gameRen.renderFloatingScores(scoreSys.getFloatingScores());
        gameRen.endWorld(shakeX, shakeY);

        // Capas de interfaz (HUD, Menú, Cuenta regresiva, Game Over)
        if (state == GameState::MODE_SELECT) {
            MenuData md;
            md.time = globalTime;
            md.anim = menuAnim;
            md.selected = selectedMode;
            md.versusUnlocked = hasTwoDualshocks;
            md.p1Connected = touchP1.isConnected();
            md.p1IsArcade = (touchP1.getController() == nullptr && touchP1.getArcadeStick() != nullptr);
            md.p2Connected = (touchP2.getController() != nullptr);
            md.best = bestScore;
            gameRen.renderMenu(md);

        } else if (state == GameState::COUNTDOWN) {
            if (isVersus) {
                Hud1v1Data hd;
                hd.timeLeft = matchTimer;
                hd.time = globalTime;
                gameRen.renderHud1v1(hd);
            } else {
                HudSoloData hd;
                hd.best = bestScore;
                hd.lives = scoreSys.getLives();
                hd.time = globalTime;
                gameRen.renderHudSolo(hd);
            }
            int num = static_cast<int>(std::ceil(countdownTimer - 0.35f));
            float beatT = (num > 0) ? (1.0f - (countdownTimer - 0.35f - (num - 1))) : (1.0f - countdownTimer / 0.35f);
            gameRen.renderCountdown(std::max(0, num), std::clamp(beatT, 0.0f, 1.0f));

        } else if (state == GameState::PLAYING) {
            if (isVersus) {
                Hud1v1Data hd;
                for (int i = 0; i < 2; ++i) {
                    hd.score[i] = scoreSys.getScore(i);
                    hd.displayScore[i] = static_cast<int>(std::round(displayScoreF[i]));
                    hd.combo[i] = scoreSys.getCombo(i);
                    hd.comboFrac[i] = scoreSys.getComboTimerFrac(i);
                    hd.scorePop[i] = scorePop[i];
                }
                hd.timeLeft = matchTimer;
                hd.time = globalTime;
                gameRen.renderHud1v1(hd);
            } else {
                HudSoloData hd;
                hd.displayScore = static_cast<int>(std::round(displayScoreF[0]));
                hd.best = bestScore;
                hd.combo = scoreSys.getCombo(0);
                hd.comboFrac = scoreSys.getComboTimerFrac(0);
                hd.lives = scoreSys.getLives();
                hd.scorePop = scorePop[0];
                hd.lifeShake = lifeShake;
                hd.time = globalTime;
                gameRen.renderHudSolo(hd);
            }

        } else if (state == GameState::GAME_OVER) {
            GameOverData gd;
            gd.time = globalTime;
            gd.anim = gameOverAnim;
            gd.versus = isVersus;
            for (int i = 0; i < 2; ++i) {
                gd.score[i] = scoreSys.getScore(i);
                gd.displayScore[i] = static_cast<int>(std::round(displayScoreF[i]));
                gd.cuts[i] = cutsCount[i];
                gd.perfects[i] = perfectsCount[i];
                gd.maxCombo[i] = maxCombo[i];
            }
            gd.best = bestScore;
            gd.newRecord = newRecordAchieved;
            gd.rank = computeRank(scoreSys.getScore(0));
            gd.rankReveal = std::clamp((gameOverAnim - 0.35f) * 2.5f, 0.0f, 1.0f);
            gameRen.renderGameOver(gd);
        }

        SDL_RenderPresent(renderer);
    }

    // Limpieza de recursos
    if (sfxSlice) Mix_FreeChunk(sfxSlice);
    if (sfxPerfect) Mix_FreeChunk(sfxPerfect);
    if (sfxExplode) Mix_FreeChunk(sfxExplode);
    if (sfxBeep) Mix_FreeChunk(sfxBeep);
    if (sfxGo) Mix_FreeChunk(sfxGo);

    if (fontGiant) TTF_CloseFont(fontGiant);
    if (fontTitleBig) TTF_CloseFont(fontTitleBig);
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
