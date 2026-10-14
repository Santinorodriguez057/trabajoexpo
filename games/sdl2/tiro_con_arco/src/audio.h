#pragma once

#include <SDL.h>
#include <SDL_mixer.h>
#include <string>
#include <unordered_map>

class SoundManager {
public:
    SoundManager();
    ~SoundManager();

    bool init();
    void playDraw(float tension);
    void playShoot();
    void playImpact();
    void playBullseye();
    void playFanfare();

private:
    std::string resolvePath(const std::string& filename);

    Mix_Chunk* sfxTensar_ = nullptr;
    Mix_Chunk* sfxDisparo_ = nullptr;
    Mix_Chunk* sfxImpacto_ = nullptr;
    Mix_Chunk* sfxDiana_ = nullptr;
    Mix_Chunk* sfxVictoria_ = nullptr;

    int drawChannel_ = -1;
    bool ready_ = false;
};
