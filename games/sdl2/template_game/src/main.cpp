// Plantilla de minijuego SDL2.
//
// - Se lanza desde el menu (o directamente).
// - Usa el mando PS4: palanca izquierda mueve el cuadrado,
//   X (A) cambia el color, Circulo (B) o ESC salen y vuelven al menu.
// - IMPORTANTE: el juego debe terminar limpiamente (return 0) para
//   que el menu recupere el control.

#include <SDL.h>

#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
        std::fprintf(stderr, "SDL_Init fallo: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* win = SDL_CreateWindow("Juego Plantilla", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, 800, 600, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!win || !ren) {
        std::fprintf(stderr, "Error creando ventana/renderer: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Abre el primer mando compatible (PS4 DualShock 4).
    SDL_GameController* gc = nullptr;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            gc = SDL_GameControllerOpen(i);
            if (gc) break;
        }
    }

    // Estado del juego
    int x = 400, y = 300;
    SDL_Color color = {80, 160, 220, 255};
    bool running = true;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = false;
            } else if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) {
                running = false; // volver al menu
            } else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
                if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
                    running = false; // Circulo: volver al menu
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                    // X: cambia el color
                    color = {static_cast<Uint8>(std::rand() % 256),
                             static_cast<Uint8>(std::rand() % 256),
                             static_cast<Uint8>(std::rand() % 256), 255};
                }
            }
        }

        // Movimiento con la palanca izquierda
        if (gc) {
            const int dx = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX);
            const int dy = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY);
            x += dx / 400;
            y += dy / 400;
        }

        // Dibujo
        SDL_SetRenderDrawColor(ren, 25, 25, 35, 255);
        SDL_RenderClear(ren);
        SDL_SetRenderDrawColor(ren, color.r, color.g, color.b, 255);
        SDL_Rect rect = {x - 25, y - 25, 50, 50};
        SDL_RenderFillRect(ren, &rect);
        SDL_RenderPresent(ren);

        SDL_Delay(16);
    }

    if (gc) SDL_GameControllerClose(gc);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}