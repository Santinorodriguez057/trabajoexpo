#pragma once

#include <SDL.h>
#include <SDL_mixer.h>

class SoundManager {
public:
    SoundManager();
    ~SoundManager();

    bool init();
    void updateEngine(float rpm, float throttle);
    void playScreech(bool active);
    void playCheckpoint();
    void playCrash();

private:
    Mix_Chunk* generateTone(int durationMs, int freq, float decay, float volume, int type);

    Mix_Chunk* sfxEngine_ = nullptr;
    Mix_Chunk* sfxScreech_ = nullptr;
    Mix_Chunk* sfxCheckpoint_ = nullptr;
    Mix_Chunk* sfxCrash_ = nullptr;

    int engineChannel_ = -1;
    int screechChannel_ = -1;
    bool ready_ = false;
};
