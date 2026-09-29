#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <string>

// Estados del juego
enum GameState {
    STATE_WAITING, 
    STATE_TENSION, 
    STATE_DRAW,    
    STATE_RESULT   
};

struct Player {
    SDL_GameController* gc = nullptr;
    bool ready = false;
    bool foul = false;
    bool has_drawn = false;
    // Vectores para la gravedad en posicion enfundado y apuntando (x, y, z)
    float accel[3] = {0,0,0};
    float gyro[3] = {0,0,0};
    float cal_down[3] = {9.8f, 0.0f, 0.0f};      // Segun las pruebas (Mando vertical: Gravedad en X)
    float cal_forward[3] = {0.0f, 0.0f, -9.8f}; // Segun las pruebas (Apuntando: Gravedad en -Z)
};

float lengthVec(const float a[3]) {
    return std::sqrt(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
}

float dotProduct(const float a[3], const float b[3]) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

bool isMatchingPose(const float current[3], const float target[3], float tolerance = 0.6f) {
    float len_c = lengthVec(current);
    float len_t = lengthVec(target);
    if (len_c < 0.1f || len_t < 0.1f) return false;
    float dot = dotProduct(current, target) / (len_c * len_t);
    return dot > tolerance; // 0.6 es aprox 53 grados de tolerancia de margen
}

static TTF_Font* loadFont() {
    const char* candidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
    };
    for (const char* path : candidates) {
        TTF_Font* f = TTF_OpenFont(path, 96);
        if (f) return f;
    }
    return nullptr;
}

void renderText(SDL_Renderer* ren, TTF_Font* font, const std::string& text, int x, int y, SDL_Color color, bool center = false, float scale = 1.0f) {
    if (!font || text.empty()) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_FreeSurface(surf);
    if (!tex) return;

    int w, h;
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    
    int dstW = static_cast<int>(w * scale);
    int dstH = static_cast<int>(h * scale);
    int dstX = center ? (x - dstW / 2) : x;
    int dstY = center ? (y - dstH / 2) : y;
    
    SDL_Rect dst = {dstX, dstY, dstW, dstH};
    SDL_RenderCopy(ren, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

int main(int argc, char** argv) {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    // Forzamos a SDL2 a usar su driver interno (hidapi) para mandos de PS4.
    // Esto habilita sensores (giroscopio/acelerometro) y vibracion en Linux
    // independientemente del driver generico del sistema.
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_SENSOR | SDL_INIT_AUDIO) < 0) {
        std::fprintf(stderr, "SDL_Init fallo: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() < 0) {
        std::fprintf(stderr, "TTF_Init fallo: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); // Anti-aliasing para escalado
    SDL_Window* win = SDL_CreateWindow("Duelo de Vaqueros", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(ren, 1920, 1080);
    
    if (!win || !ren) {
        std::fprintf(stderr, "Error creando ventana/renderer: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    TTF_Font* font = loadFont();

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::fprintf(stderr, "Mix_OpenAudio fallo: %s\n", Mix_GetError());
    }

    SDL_AudioSpec wav_spec;
    Uint32 wav_length = 0;
    Uint8 *wav_buffer = nullptr;
    SDL_AudioDeviceID controller_audio = 0;
    
    if (SDL_LoadWAV("games/sdl2/duelo_vaqueros/assets/disparo.wav", &wav_spec, &wav_buffer, &wav_length) == NULL) {
        std::fprintf(stderr, "No se pudo cargar disparo.wav: %s\n", SDL_GetError());
    } else {
        int count = SDL_GetNumAudioDevices(0);
        for (int i = 0; i < count; ++i) {
            const char* name = SDL_GetAudioDeviceName(i, 0);
            if (std::string(name).find("Wireless Controller") != std::string::npos ||
                std::string(name).find("DualShock") != std::string::npos) {
                controller_audio = SDL_OpenAudioDevice(name, 0, &wav_spec, NULL, 0);
                if (controller_audio > 0) break;
            }
        }
    }
    
    Mix_Chunk* sfxDisparoPC = Mix_LoadWAV("games/sdl2/duelo_vaqueros/assets/disparo.wav");
    Mix_Chunk* bgmTension = Mix_LoadWAV("games/sdl2/duelo_vaqueros/assets/tension.wav");

    auto playFireSound = [&]() {
        if (sfxDisparoPC) {
            Mix_PlayChannel(-1, sfxDisparoPC, 0); // Siempre por la PC
        }
        if (controller_audio > 0 && wav_buffer != nullptr) {
            SDL_ClearQueuedAudio(controller_audio);
            SDL_QueueAudio(controller_audio, wav_buffer, wav_length);
            SDL_PauseAudioDevice(controller_audio, 0); // Y tambien por el mando si se puede
        }
    };

    Player p1, p2;
    GameState state = STATE_WAITING;
    Uint32 tension_start_time = 0;
    Uint32 tension_duration = 0;
    
    int winner = 0; // 0: nadie, 1: J1, 2: J2

    bool running = true;
    while (running) {
        // Asignar mandos si no estan asignados
        if (!p1.gc || !p2.gc) {
            for (int i = 0; i < SDL_NumJoysticks(); ++i) {
                if (SDL_IsGameController(i)) {
                    SDL_GameController* gc = SDL_GameControllerOpen(i);
                    if (gc) {
                        if (!p1.gc) {
                            p1.gc = gc;
                            SDL_GameControllerSetSensorEnabled(p1.gc, SDL_SENSOR_GYRO, SDL_TRUE);
                            SDL_GameControllerSetSensorEnabled(p1.gc, SDL_SENSOR_ACCEL, SDL_TRUE);
                        } else if (!p2.gc && p1.gc != gc) {
                            p2.gc = gc;
                            SDL_GameControllerSetSensorEnabled(p2.gc, SDL_SENSOR_GYRO, SDL_TRUE);
                            SDL_GameControllerSetSensorEnabled(p2.gc, SDL_SENSOR_ACCEL, SDL_TRUE);
                        }
                    }
                }
            }
        }

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = false;
            } else if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            } else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
                if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
                    running = false; // Circulo para salir
                }
                
                if (state == STATE_WAITING && ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                    SDL_JoystickID id = ev.cbutton.which;
                    SDL_JoystickID id1 = p1.gc ? SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(p1.gc)) : -1;
                    SDL_JoystickID id2 = p2.gc ? SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(p2.gc)) : -1;
                    
                    if (id == id1 && isMatchingPose(p1.accel, p1.cal_down, 0.5f)) p1.ready = true;
                    if (id == id2 && isMatchingPose(p2.accel, p2.cal_down, 0.5f)) p2.ready = true;
                }
                
                if (state == STATE_RESULT && ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                    state = STATE_WAITING;
                    p1.ready = p2.ready = p1.foul = p2.foul = false;
                    winner = 0;
                }
            }
        }

        // Variables de debug para verlas en pantalla
        static float debug_gyro[3] = {0,0,0};
        static float debug_accel[3] = {0,0,0};
        static Sint16 debug_trigger = 0;

        auto readSensors = [](Player& p) {
            if (!p.gc) return;
            if (SDL_GameControllerHasSensor(p.gc, SDL_SENSOR_GYRO)) {
                SDL_GameControllerGetSensorData(p.gc, SDL_SENSOR_GYRO, p.gyro, 3);
            }
            if (SDL_GameControllerHasSensor(p.gc, SDL_SENSOR_ACCEL)) {
                SDL_GameControllerGetSensorData(p.gc, SDL_SENSOR_ACCEL, p.accel, 3);
            }
        };

        auto checkDrawn = [](Player& p) {
            if (!p.gc) return;
            // Movimiento brusco del giroscopio (Subimos de 2.0 a 4.0 para que no sea tan sensible)
            if (std::abs(p.gyro[0]) > 4.0f || std::abs(p.gyro[1]) > 4.0f || std::abs(p.gyro[2]) > 4.0f) {
                p.has_drawn = true;
            }
            // O si la gravedad coincide con la posicion apuntando al frente
            // Tolerancia a 0.8f (antes 0.5f) exige que el mando esté mucho más cerca de la horizontal ideal
            if (isMatchingPose(p.accel, p.cal_forward, 0.8f)) {
                p.has_drawn = true;
            }
        };

        auto hasPulledTrigger = [](Player& p) -> bool {
            if (!p.gc) return false;
            debug_trigger = SDL_GameControllerGetAxis(p.gc, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
            return debug_trigger > 10000;
        };

        readSensors(p1);
        readSensors(p2);
        
        // Copiamos a variables estaticas para el texto debug (mostramos lo del J1)
        debug_gyro[0] = p1.gyro[0]; debug_gyro[1] = p1.gyro[1]; debug_gyro[2] = p1.gyro[2];
        debug_accel[0] = p1.accel[0]; debug_accel[1] = p1.accel[1]; debug_accel[2] = p1.accel[2];

        if (state == STATE_WAITING) {
            // Reiniciamos el estado de desenfunde mientras esperamos
            p1.has_drawn = false;
            p2.has_drawn = false;
            
            int mandos_conectados = 0;
            for (int i = 0; i < SDL_NumJoysticks(); ++i) {
                if (SDL_IsGameController(i)) mandos_conectados++;
            }

            bool p1_listo = p1.ready;
            bool p2_listo = p2.ready;

            bool listos_para_empezar = (p1_listo && p2_listo);
            if (mandos_conectados <= 1 && p1_listo) {
                listos_para_empezar = true; 
            }

            if (listos_para_empezar) {
                state = STATE_TENSION;
                tension_start_time = SDL_GetTicks();
                tension_duration = 2000 + (std::rand() % 4000);
                if (bgmTension) Mix_PlayChannel(1, bgmTension, -1);
            }
        }
        else if (state == STATE_TENSION) {
            checkDrawn(p1);
            checkDrawn(p2);
            
            // En tension, moverse (desenfundar) o disparar es falta
            bool p1_foul = p1.has_drawn || hasPulledTrigger(p1);
            bool p2_foul = p2.has_drawn || hasPulledTrigger(p2);
            
            if (p1_foul || p2_foul) {
                playFireSound();
                Mix_HaltChannel(1);
                state = STATE_RESULT;
                if (p1_foul) { p1.foul = true; winner = 2; }
                else { p2.foul = true; winner = 1; }
            } else if (SDL_GetTicks() - tension_start_time > tension_duration) {
                state = STATE_DRAW;
                if (p1.gc) SDL_GameControllerRumble(p1.gc, 0xFFFF, 0xFFFF, 400);
                if (p2.gc) SDL_GameControllerRumble(p2.gc, 0xFFFF, 0xFFFF, 400);
                Mix_HaltChannel(1);
            }
        }
        else if (state == STATE_DRAW) {
            checkDrawn(p1);
            checkDrawn(p2);
            
            // Para disparar, TIENE que haber desenfundado (has_drawn) Y apretar gatillo
            bool p1_fire = p1.has_drawn && hasPulledTrigger(p1);
            bool p2_fire = p2.has_drawn && hasPulledTrigger(p2);
            
            if (p1_fire && !p2_fire) {
                playFireSound(); winner = 1; state = STATE_RESULT;
            } else if (p2_fire && !p1_fire) {
                playFireSound(); winner = 2; state = STATE_RESULT;
            } else if (p1_fire && p2_fire) {
                playFireSound(); winner = 3; state = STATE_RESULT; 
            }
        }

        // --- RENDERIZADO NEO-BRUTALISTA B&W ---
        if (state == STATE_TENSION) {
            SDL_SetRenderDrawColor(ren, 15, 15, 15, 255); // Fondo oscuro en tension
        } else {
            SDL_SetRenderDrawColor(ren, 250, 250, 250, 255); // Fondo blanco por defecto
        }
        SDL_RenderClear(ren);

        // Cuadricula de fondo (solo si no estamos en tension oscura)
        if (state != STATE_TENSION) {
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 15);
            for (int x = 0; x < 1920; x += 60) SDL_RenderDrawLine(ren, x, 0, x, 1080);
            for (int y = 0; y < 1080; y += 60) SDL_RenderDrawLine(ren, 0, y, 1920, y);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
        }

        // Utilidad para dibujar cajas brutalistas
        auto drawBrutalistBox = [&](int x, int y, int w, int h, bool filled) {
            // Sombra
            SDL_SetRenderDrawColor(ren, 10, 10, 10, 255);
            SDL_Rect shadow = {x + 16, y + 16, w, h};
            SDL_RenderFillRect(ren, &shadow);
            
            // Fondo
            SDL_SetRenderDrawColor(ren, filled ? 10 : 250, filled ? 10 : 250, filled ? 10 : 250, 255);
            SDL_Rect box = {x, y, w, h};
            SDL_RenderFillRect(ren, &box);
            
            // Borde
            SDL_SetRenderDrawColor(ren, 10, 10, 10, 255);
            for (int b = 0; b < 6; ++b) {
                SDL_Rect border = {x + b, y + b, w - b*2, h - b*2};
                SDL_RenderDrawRect(ren, &border);
            }
        };

        if (state == STATE_WAITING) {
            renderText(ren, font, "DUELO DE VAQUEROS", 960, 150, {10, 10, 10, 255}, true, 1.2f);
            
            auto drawPlayer = [&](Player& p, int cx, std::string label) {
                if (!p.gc) return;
                bool correctPose = isMatchingPose(p.accel, p.cal_down, 0.5f);
                
                int boxW = 400;
                int boxH = 400;
                int boxX = cx - boxW / 2;
                int boxY = 350;
                
                drawBrutalistBox(boxX, boxY, boxW, boxH, p.ready);
                
                SDL_Color textColor = p.ready ? SDL_Color{250, 250, 250, 255} : SDL_Color{10, 10, 10, 255};
                
                renderText(ren, font, label, cx, boxY + 100, textColor, true, 0.7f);
                
                std::string status = "APUNTA ABAJO";
                if (p.ready) status = "LISTO";
                else if (correctPose) status = "PULSA X";
                
                renderText(ren, font, status, cx, boxY + 280, textColor, true, 0.5f);
            };
            
            drawPlayer(p1, 500, "JUGADOR 1");
            if (p2.gc) drawPlayer(p2, 1420, "JUGADOR 2");
            else renderText(ren, font, "ESPERANDO JUGADOR 2...", 1420, 550, {100, 100, 100, 255}, true, 0.5f);
        }
        else if (state == STATE_TENSION) {
            renderText(ren, font, "PREPARADOS...", 960, 540, {250, 250, 250, 255}, true, 1.5f);
        }
        else if (state == STATE_DRAW) {
            // Flashbang gigante visual para reaccionar
            renderText(ren, font, "¡FUEGO!", 960, 540, {10, 10, 10, 255}, true, 4.0f);
        }
        else if (state == STATE_RESULT) {
            // Tarjeta central brutalista para el resultado
            drawBrutalistBox(460, 340, 1000, 400, false);
            
            std::string mainText = "";
            std::string subText = "";
            
            if (winner == 1) {
                mainText = "¡GANA J1!";
                if (p2.foul) subText = "EL JUGADOR 2 DISPARO ANTES (FALTA)";
            } else if (winner == 2) {
                mainText = "¡GANA J2!";
                if (p1.foul) subText = "EL JUGADOR 1 DISPARO ANTES (FALTA)";
            } else {
                mainText = "¡EMPATE!";
            }
            
            renderText(ren, font, mainText, 960, 480, {10, 10, 10, 255}, true, 2.0f);
            if (!subText.empty()) renderText(ren, font, subText, 960, 600, {80, 80, 80, 255}, true, 0.4f);
            
            renderText(ren, font, "X: Revancha  |  Circulo: Salir", 960, 680, {10, 10, 10, 255}, true, 0.4f);
        }

        // --- DEBUG INFO ---
        char debugStr[128];
        std::snprintf(debugStr, sizeof(debugStr), "Debug - Trigger: %d | Gyro: %.1f | Accel: %.1f, %.1f | Drawn: %s", 
                      debug_trigger, debug_gyro[0], debug_accel[1], debug_accel[2], p1.has_drawn ? "SI" : "NO");
        renderText(ren, font, debugStr, 20, 20, {150, 150, 150, 255}, false, 0.25f);

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    if (font) TTF_CloseFont(font);
    
    if (wav_buffer) SDL_FreeWAV(wav_buffer);
    if (controller_audio > 0) SDL_CloseAudioDevice(controller_audio);
    if (sfxDisparoPC) Mix_FreeChunk(sfxDisparoPC);
    if (bgmTension) Mix_FreeChunk(bgmTension);
    Mix_CloseAudio();

    if (p1.gc) SDL_GameControllerClose(p1.gc);
    if (p2.gc) SDL_GameControllerClose(p2.gc);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
    return 0;
}