#include <SDL.h>
#include <SDL_ttf.h>
#include <cstdio>
#include <string>
#include <vector>

static TTF_Font* loadFont() {
    const char* candidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
    };
    for (const char* path : candidates) {
        TTF_Font* f = TTF_OpenFont(path, 20);
        if (f) return f;
    }
    return nullptr;
}

void renderText(SDL_Renderer* ren, TTF_Font* font, const std::string& text, int x, int y, SDL_Color color) {
    if (!font || text.empty()) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_FreeSurface(surf);
    if (!tex) return;
    int w, h;
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    SDL_Rect dst = {x, y, w, h};
    SDL_RenderCopy(ren, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

int main(int argc, char** argv) {
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_SENSOR) < 0) return 1;
    if (TTF_Init() < 0) return 1;

    SDL_Window* win = SDL_CreateWindow("Test de Mando", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    TTF_Font* font = loadFont();

    SDL_GameController* gc = nullptr;

    bool running = true;
    while (running) {
        if (!gc) {
            for (int i = 0; i < SDL_NumJoysticks(); ++i) {
                if (SDL_IsGameController(i)) {
                    gc = SDL_GameControllerOpen(i);
                    if (gc) {
                        SDL_GameControllerSetSensorEnabled(gc, SDL_SENSOR_GYRO, SDL_TRUE);
                        SDL_GameControllerSetSensorEnabled(gc, SDL_SENSOR_ACCEL, SDL_TRUE);
                        break;
                    }
                }
            }
        }

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) running = false;
            if (ev.type == SDL_CONTROLLERBUTTONDOWN && ev.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
                running = false;
            }
            if (ev.type == SDL_CONTROLLERBUTTONDOWN && ev.cbutton.button == SDL_CONTROLLER_BUTTON_X) {
                if (gc) SDL_GameControllerRumble(gc, 0xFFFF, 0xFFFF, 200); // Vibrar de prueba
            }
        }

        SDL_SetRenderDrawColor(ren, 20, 20, 30, 255);
        SDL_RenderClear(ren);

        if (!gc) {
            renderText(ren, font, "Conecta un mando...", 50, 50, {255,255,255,255});
        } else {
            std::vector<std::string> lines;
            lines.push_back("--- EJES ---");
            lines.push_back("Izquierdo X: " + std::to_string(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX)));
            lines.push_back("Izquierdo Y: " + std::to_string(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY)));
            lines.push_back("Derecho X: " + std::to_string(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTX)));
            lines.push_back("Derecho Y: " + std::to_string(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTY)));
            lines.push_back("Gatillo L2: " + std::to_string(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERLEFT)));
            lines.push_back("Gatillo R2: " + std::to_string(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERRIGHT)));

            lines.push_back("");
            lines.push_back("--- SENSORES ---");
            float gyro[3] = {0,0,0};
            float accel[3] = {0,0,0};
            
            bool hasGyro = SDL_GameControllerHasSensor(gc, SDL_SENSOR_GYRO);
            bool hasAccel = SDL_GameControllerHasSensor(gc, SDL_SENSOR_ACCEL);
            
            if (hasGyro) SDL_GameControllerGetSensorData(gc, SDL_SENSOR_GYRO, gyro, 3);
            if (hasAccel) SDL_GameControllerGetSensorData(gc, SDL_SENSOR_ACCEL, accel, 3);
            
            lines.push_back("Gyro Detectado: " + std::string(hasGyro ? "SI" : "NO"));
            lines.push_back("Gyro X: " + std::to_string(gyro[0]));
            lines.push_back("Gyro Y: " + std::to_string(gyro[1]));
            lines.push_back("Gyro Z: " + std::to_string(gyro[2]));
            
            lines.push_back("Accel Detectado: " + std::string(hasAccel ? "SI" : "NO"));
            lines.push_back("Accel X: " + std::to_string(accel[0]));
            lines.push_back("Accel Y: " + std::to_string(accel[1]));
            lines.push_back("Accel Z: " + std::to_string(accel[2]));

            lines.push_back("");
            lines.push_back("Presiona TRIANGULO (Y) para vibrar / CIRCULO para salir.");

            int y = 30;
            for (const auto& l : lines) {
                renderText(ren, font, l, 50, y, {200,255,200,255});
                y += 25;
            }
        }

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    if (gc) SDL_GameControllerClose(gc);
    if (font) TTF_CloseFont(font);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
    return 0;
}