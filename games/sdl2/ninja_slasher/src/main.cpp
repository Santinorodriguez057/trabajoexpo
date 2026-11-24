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
#include "sound.h"

namespace {
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

    int ratingToAudioCode(CutRating r) {
        if (r == CutRating::PERFECT) return 3;
        if (r == CutRating::GOOD) return 2;
        return 1; // GRAZE
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

    SoundSystem sound;
    sound.init();
    sound.startMenuBgm();

    const int kWidth = 1920;
    const int kHeight = 1080;

    SDL_Window* window = SDL_CreateWindow("Ninja Slasher",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          kWidth, kHeight,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window) {
        sound.shutdown();
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_DestroyWindow(window);
        sound.shutdown();
        TTF_Quit();
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
    bool isPaused = false;
    int pauseSelected = 0; // 0 = Continuar, 1 = Salir del juego
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
    float empFlashTimer = 0.0f;
    float dangerPulseTimer = 0.0f;
    float gameOverAnim = 0.0f;

    // Estadísticas y contadores animados (Game-Feel)
    float displayScoreF[2] = {0.0f, 0.0f};
    float scorePop[2] = {0.0f, 0.0f};
    float lifeShake = 0.0f;
    int cutsCount[2] = {0, 0};
    int perfectsCount[2] = {0, 0};
    int maxCombo[2] = {0, 0};
    int bestScore = ScoreSystem::loadBestScore();
    bool newRecordAchieved = false;
    bool prevBtnRestart = false;
    float menuDemoTimer = 0.8f;

    // Ventana de ráfaga para Multi-Slice ("¡TAJO MÚLTIPLE x2 / x3 / x4!")
    int burstSwipeId[2] = {-1, -1};
    int burstCount[2] = {0, 0};
    int burstAnnounced[2] = {1, 1};
    float burstTimer[2] = {0.0f, 0.0f};

    // Temporizadores de Power-ups
    float freezeTimer = 0.0f;
    float dualTimer[2] = {0.0f, 0.0f};
    float frenzyTimer[2] = {0.0f, 0.0f};

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
        sf.length = res.isMine ? 290.0f : (isPerf ? 265.0f : 200.0f);
        sf.life = 1.0f;
        sf.color = res.isMine ? CyberTheme::cRed : (isPerf ? CyberTheme::cYellow : pCol);
        flashes.push_back(sf);

        // Onda expansiva neobrutalista
        ShockRing sr;
        sr.x = e.getX();
        sr.y = e.getY();
        sr.radius = e.getRadius() * 0.55f;
        sr.maxRadius = res.isMine ? 225.0f : (isPerf ? 170.0f : 120.0f);
        sr.thickness = res.isMine ? 16.0f : 12.0f;
        sr.life = 1.0f;
        sr.color = res.isMine ? CyberTheme::cRed : (isPerf ? CyberTheme::cGreen : pCol);
        rings.push_back(sr);

        // Chispas geométricas rotatorias con borde negro
        int numSparks = res.isMine ? 28 : (isPerf ? 22 : 14);
        for (int p = 0; p < numSparks; ++p) {
            SparkParticle sp;
            sp.x = e.getX();
            sp.y = e.getY();
            float a = (std::rand() % 628) / 100.0f;
            float s = 180.0f + (std::rand() % 430);
            sp.vx = std::cos(a) * s;
            sp.vy = std::sin(a) * s - 85.0f;
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
        empFlashTimer = 0.0f;
        dangerPulseTimer = 0.0f;
        cutsCount[0] = cutsCount[1] = 0;
        perfectsCount[0] = perfectsCount[1] = 0;
        maxCombo[0] = maxCombo[1] = 0;
        for (int p = 0; p < 2; ++p) {
            burstSwipeId[p] = -1;
            burstCount[p] = 0;
            burstAnnounced[p] = 1;
            burstTimer[p] = 0.0f;
        }
        newRecordAchieved = false;
        freezeTimer = 0.0f;
        dualTimer[0] = dualTimer[1] = 0.0f;
        frenzyTimer[0] = frenzyTimer[1] = 0.0f;
        touchP1.setDualBlade(false);
        touchP2.setDualBlade(false);
        sound.startGameBgm();
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

        // Actualizar temporizadores de power-ups y multi-slice (congelados mientras el menú de pausa está abierto)
        if (!isPaused) {
            if (freezeTimer > 0.0f) freezeTimer = std::max(0.0f, freezeTimer - dtReal);
            if (empFlashTimer > 0.0f) empFlashTimer = std::max(0.0f, empFlashTimer - dtReal * 2.8f);
            for (int p = 0; p < 2; ++p) {
                if (dualTimer[p] > 0.0f) {
                    dualTimer[p] = std::max(0.0f, dualTimer[p] - dtReal);
                    if (p == 0) touchP1.setDualBlade(dualTimer[0] > 0.0f);
                    else touchP2.setDualBlade(dualTimer[1] > 0.0f);
                }
                if (frenzyTimer[p] > 0.0f) frenzyTimer[p] = std::max(0.0f, frenzyTimer[p] - dtReal);
                if (burstTimer[p] > 0.0f) {
                    burstTimer[p] = std::max(0.0f, burstTimer[p] - dtReal);
                    if (burstTimer[p] <= 0.0f) {
                        burstCount[p] = 0;
                        burstAnnounced[p] = 1;
                    }
                }
            }
        }

        // Atenuar música durante pausa, cámara lenta o congelación
        sound.setBgmDuck(isPaused || freezeTimer > 0.0f || slowMoTimer > 0.0f);

        // Hit-stop (micro-pausa de impacto) y cámara lenta cinematográfica
        float dt = isPaused ? 0.0f : dtReal;
        if (!isPaused) {
            if (hitStopTimer > 0.0f) {
                hitStopTimer -= dtReal;
                dt = 0.0f;
            } else if (slowMoTimer > 0.0f || freezeTimer > 0.0f) {
                if (slowMoTimer > 0.0f) slowMoTimer -= dtReal;
                dt *= (freezeTimer > 0.0f) ? 0.30f : 0.35f;
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

            // Menú de Pausa con botón OPTIONS (START) o ESC
            if (isPaused) {
                if (ev.type == SDL_KEYDOWN) {
                    if (ev.key.keysym.sym == SDLK_UP || ev.key.keysym.sym == SDLK_w ||
                        ev.key.keysym.sym == SDLK_DOWN || ev.key.keysym.sym == SDLK_s) {
                        pauseSelected = (pauseSelected + 1) % 2;
                        sound.playUiNav();
                    } else if (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_SPACE) {
                        sound.playUiConfirm();
                        if (pauseSelected == 0) {
                            isPaused = false;
                            prevBtnRestart = true;
                        } else {
                            running = false;
                        }
                    } else if (ev.key.keysym.sym == SDLK_ESCAPE) {
                        sound.playUiNav();
                        isPaused = false;
                    }
                } else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP ||
                        ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
                        pauseSelected = (pauseSelected + 1) % 2;
                        sound.playUiNav();
                    } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                        sound.playUiConfirm();
                        if (pauseSelected == 0) {
                            isPaused = false;
                            prevBtnRestart = true;
                        } else {
                            running = false;
                        }
                    } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_B ||
                               ev.cbutton.button == SDL_CONTROLLER_BUTTON_START ||
                               ev.cbutton.button == SDL_CONTROLLER_BUTTON_BACK) {
                        sound.playUiNav();
                        isPaused = false;
                    }
                } else if (ev.type == SDL_JOYBUTTONDOWN) {
                    if (ev.jbutton.button == 0) {
                        sound.playUiConfirm();
                        if (pauseSelected == 0) {
                            isPaused = false;
                            prevBtnRestart = true;
                        } else {
                            running = false;
                        }
                    }
                }
                continue;
            }

            // Tecla / botón de pausa o navegación en menú
            if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_ESCAPE) {
                    isPaused = true;
                    pauseSelected = 0;
                    sound.playUiNav();
                } else if (state == GameState::MODE_SELECT) {
                    if (ev.key.keysym.sym == SDLK_LEFT && selectedMode != 0) {
                        selectedMode = 0;
                        sound.playUiNav();
                    }
                    if (ev.key.keysym.sym == SDLK_RIGHT && hasTwoDualshocks && selectedMode != 1) {
                        selectedMode = 1;
                        sound.playUiNav();
                    }
                    if (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_SPACE) {
                        menuConfirmPressed = true;
                    }
                }
            } else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
                if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_BACK || ev.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                    isPaused = true;
                    pauseSelected = 0;
                    touchP1.triggerHaptic(0.0f, 0.0f, 0);
                    touchP2.triggerHaptic(0.0f, 0.0f, 0);
                    sound.playUiNav();
                } else if (state == GameState::MODE_SELECT) {
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT && selectedMode != 0) {
                        selectedMode = 0;
                        sound.playUiNav();
                    } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT && hasTwoDualshocks && selectedMode != 1) {
                        selectedMode = 1;
                        sound.playUiNav();
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
        if (isPaused) {
            // El juego permanece completamente congelado hasta continuar o salir
        } else if (state == GameState::MODE_SELECT) {
            menuAnim = std::min(1.0f, menuAnim + dtReal * 2.2f);
            touchP1.update(dtReal, kWidth, kHeight);
            touchP2.update(dtReal, kWidth, kHeight);

            if (touchP1.consumeWhooshPulse()) sound.playWhoosh(0, false);
            if (hasTwoDualshocks && touchP2.consumeWhooshPulse()) sound.playWhoosh(1, false);

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

            int prevMode = selectedMode;
            if (hasTwoDualshocks && touchP1.isFingerDown()) {
                selectedMode = (touchP1.getFingerX() < kWidth * 0.5f) ? 0 : 1;
            } else if (hasTwoDualshocks && touchP2.isFingerDown()) {
                selectedMode = 1;
            }
            if (selectedMode != prevMode) sound.playUiNav();

            for (const auto& slash : demoSlashes) {
                for (auto& e : entities) {
                    if (!e.isAlive()) continue;
                    CollisionResult res = CollisionSystem::evaluateSlash(slash, e);
                    if (res.hit) {
                        e.kill();
                        e.spawnHalves(res.cutAngle, halves);
                        spawnCutEffects(e, slash, res);
                        sound.playSlice(ratingToAudioCode(res.rating), e.getType() == EntityType::MECHA_SHURIKEN);
                        triggerShake(8.0f, 0.12f);
                    }
                }
            }
            entities.erase(std::remove_if(entities.begin(), entities.end(),
                                          [](const CyberEntity& e) { return !e.isAlive(); }),
                           entities.end());

            if (menuConfirmPressed) {
                sound.playUiConfirm();
                startCountdown(selectedMode == 1 && hasTwoDualshocks);
            }

        } else if (state == GameState::COUNTDOWN) {
            touchP1.update(dtReal, kWidth, kHeight);
            if (isVersus) touchP2.update(dtReal, kWidth, kHeight);

            countdownTimer -= dtReal;
            int curNum = static_cast<int>(std::ceil(countdownTimer - 0.35f));
            if (curNum < lastCountdownBeep && curNum >= 0) {
                lastCountdownBeep = curNum;
                sound.playCountdownBeep(curNum == 0);
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
                    sound.playGameOver(scoreSys.getScore(0) != scoreSys.getScore(1));
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
                    if (newRecordAchieved) {
                        ScoreSystem::saveBestScore(bestScore);
                    }
                    sound.playGameOver(newRecordAchieved);
                }
            }

            touchP1.update(dtReal, kWidth, kHeight);
            if (touchP1.consumeWhooshPulse()) sound.playWhoosh(0, frenzyTimer[0] > 0.0f);
            if (isVersus) {
                touchP2.update(dtReal, kWidth, kHeight);
                if (touchP2.consumeWhooshPulse()) sound.playWhoosh(1, frenzyTimer[1] > 0.0f);
            }

            // Latido de alerta sonora cuando queda 1 vida o últimos 8 segundos en Versus
            bool criticalDanger = (!isVersus && scoreSys.getLives() == 1) || (isVersus && matchTimer <= 8.0f && matchTimer > 0.2f);
            if (criticalDanger && state == GameState::PLAYING) {
                dangerPulseTimer -= dtReal;
                if (dangerPulseTimer <= 0.0f) {
                    dangerPulseTimer = 1.05f;
                    sound.playDangerHeartbeat();
                }
            } else {
                dangerPulseTimer = 0.0f;
            }

            spawner.update(dt, entities);

            for (auto& e : entities) {
                e.update(dt);
                // En Modo Solo: dejar caer un objeto puntuable (que no sea mina PEM) quita 1 vida
                if (!isVersus && e.isDropped() && e.getType() != EntityType::EMP_MINE) {
                    scoreSys.registerDropMiss(e.getX(), kHeight - 80.0f);
                    lifeShake = 1.0f;
                    empFlashTimer = 0.45f;
                    triggerShake(20.0f, 0.25f);
                    sound.playDropMiss();
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
                            burstCount[pIdx] = 0;
                            burstAnnounced[pIdx] = 1;
                            lifeShake = 1.0f;
                            empFlashTimer = 1.0f;
                            hitStopTimer = 0.06f;
                            triggerShake(30.0f, 0.34f);
                            sound.playMineExplosion();
                            activeTouch.triggerHaptic(0.95f, 0.95f, 360);
                            activeTouch.setLED(239, 68, 68);
                        } else {
                            // ¿Es un power-up especial?
                            bool isPowerup = false;
                            int powerupKind = 0;
                            if (e.getType() == EntityType::POWERUP_FREEZE) {
                                freezeTimer = 4.2f;
                                isPowerup = true;
                                powerupKind = 0;
                            } else if (e.getType() == EntityType::POWERUP_DUAL) {
                                dualTimer[pIdx] = 6.5f;
                                if (pIdx == 0) touchP1.setDualBlade(true);
                                else touchP2.setDualBlade(true);
                                isPowerup = true;
                                powerupKind = 1;
                            } else if (e.getType() == EntityType::POWERUP_FRENZY) {
                                frenzyTimer[pIdx] = 5.5f;
                                isPowerup = true;
                                powerupKind = 2;
                            }

                            int finalPts = res.points;
                            if (frenzyTimer[pIdx] > 0.0f) finalPts *= 2; // Doble de puntos en frenesí

                            bool isPerf = (res.rating == CutRating::PERFECT);
                            scoreSys.addCut(pIdx, finalPts, isPerf, e.getX(), e.getY());
                            cutsCount[pIdx]++;
                            if (isPerf) perfectsCount[pIdx]++;
                            maxCombo[pIdx] = std::max(maxCombo[pIdx], scoreSys.getCombo(pIdx));
                            scorePop[pIdx] = 1.0f;

                            // Detección de Multi-Slice (cortar 2 o más objetivos en un mismo gesto rápido)
                            int curSwipe = activeTouch.getSwipeId();
                            if (curSwipe == burstSwipeId[pIdx] && burstTimer[pIdx] > 0.0f) {
                                burstCount[pIdx]++;
                            } else {
                                burstSwipeId[pIdx] = curSwipe;
                                burstCount[pIdx] = 1;
                                burstAnnounced[pIdx] = 1;
                            }
                            burstTimer[pIdx] = 0.24f;

                            if (burstCount[pIdx] >= 2 && burstCount[pIdx] > burstAnnounced[pIdx]) {
                                burstAnnounced[pIdx] = burstCount[pIdx];
                                scoreSys.addMultiSliceBonus(pIdx, burstCount[pIdx], e.getX(), e.getY());
                                sound.playMultiSlice(burstCount[pIdx]);
                                triggerShake(18.0f + burstCount[pIdx] * 3.0f, 0.22f);
                                hitStopTimer = std::max(hitStopTimer, 0.045f);
                            }

                            if (!isVersus && scoreSys.getScore(0) > bestScore) {
                                bestScore = scoreSys.getScore(0);
                                newRecordAchieved = true;
                            }

                            // Audio de impacto por capas: golpe de katana + nota de racha / power-up
                            sound.playSlice(ratingToAudioCode(res.rating), e.getType() == EntityType::MECHA_SHURIKEN);

                            if (isPowerup) {
                                sound.playPowerup(powerupKind);
                                activeTouch.triggerHaptic(0.85f, 0.85f, 260);
                                triggerShake(16.0f, 0.22f);
                            } else {
                                int curCombo = scoreSys.getCombo(pIdx);
                                if (curCombo >= 2) {
                                    sound.playKillstreak(pIdx, curCombo);
                                }
                            }

                            if (isPerf) {
                                hitStopTimer = std::max(hitStopTimer, 0.04f);
                                if (freezeTimer <= 0.0f) slowMoTimer = 0.16f;
                                triggerShake(16.0f, 0.18f);
                                activeTouch.triggerHaptic(0.5f, 0.8f, 180);
                                activeTouch.setLED(34, 197, 94);
                            } else {
                                triggerShake(9.0f, 0.12f);
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

            // Reiniciar con botón [X] (Cruz / Button A), botón 0 del arcade stick o Enter/Espacio
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
            const Uint8* keys = SDL_GetKeyboardState(nullptr);
            if (keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_SPACE]) {
                btnDown = true;
            }

            if (btnDown && !prevBtnRestart && gameOverAnim > 0.55f) {
                sound.playUiConfirm();
                sound.startMenuBgm();
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

        if (!isPaused) {
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
        }

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
        float activeFrenzy = std::max(frenzyTimer[0], frenzyTimer[1]);

        gameRen.beginFrame();
        gameRen.beginWorld();

        gameRen.renderBackground(globalTime, accentLevel, inDanger, freezeTimer > 0.0f, activeFrenzy > 0.0f);
        gameRen.renderRings(rings);
        gameRen.renderEntities(entities, globalTime);
        gameRen.renderHalves(halves);
        gameRen.renderParticles(particles);
        gameRen.renderSlashFlashes(flashes);

        // Estelas de katana + indicador del dedo en el touchpad
        gameRen.renderTrails(touchP1.getTrail(), CyberTheme::cP1Trail,
                             touchP1.isFingerDown(), touchP1.getFingerX(), touchP1.getFingerY(),
                             globalTime, frenzyTimer[0] > 0.0f, dualTimer[0] > 0.0f);
        if (isVersus || (state == GameState::MODE_SELECT && hasTwoDualshocks)) {
            gameRen.renderTrails(touchP2.getTrail(), CyberTheme::cP2Trail,
                                 touchP2.isFingerDown(), touchP2.getFingerX(), touchP2.getFingerY(),
                                 globalTime, frenzyTimer[1] > 0.0f, dualTimer[1] > 0.0f);
        }

        gameRen.renderFloatingScores(scoreSys.getFloatingScores());
        gameRen.renderMultiSliceBanners(scoreSys.getMultiBanners(), globalTime);
        gameRen.endWorld(shakeX, shakeY);

        // Efectos atmosféricos de pantalla completa (Escarcha, Frenesí, Glitch PEM)
        if (state == GameState::PLAYING) {
            gameRen.renderScreenOverlays(globalTime, freezeTimer, activeFrenzy, empFlashTimer);
        }

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
                    hd.dualTimer[i] = dualTimer[i];
                    hd.frenzyTimer[i] = frenzyTimer[i];
                }
                hd.timeLeft = matchTimer;
                hd.time = globalTime;
                hd.freezeTimer = freezeTimer;
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
                hd.freezeTimer = freezeTimer;
                hd.dualTimer = dualTimer[0];
                hd.frenzyTimer = frenzyTimer[0];
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

        if (isPaused) {
            gameRen.renderPauseMenu(pauseSelected, globalTime);
        }

        SDL_RenderPresent(renderer);
    }

    // Guardar récord final por seguridad al salir
    if (bestScore > 0) {
        ScoreSystem::saveBestScore(bestScore);
    }

    if (fontGiant) TTF_CloseFont(fontGiant);
    if (fontTitleBig) TTF_CloseFont(fontTitleBig);
    if (fontTitle) TTF_CloseFont(fontTitle);
    if (fontHud) TTF_CloseFont(fontHud);
    if (fontSmall) TTF_CloseFont(fontSmall);

    sound.shutdown();
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
