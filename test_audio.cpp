#include <SDL2/SDL.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_AUDIO);
    int count = SDL_GetNumAudioDevices(0);
    printf("Playback devices: %d\n", count);
    for (int i = 0; i < count; ++i) {
        printf("Device %d: %s\n", i, SDL_GetAudioDeviceName(i, 0));
    }
    SDL_Quit();
    return 0;
}
