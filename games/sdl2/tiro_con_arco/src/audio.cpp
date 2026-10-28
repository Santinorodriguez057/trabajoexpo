#include "audio.h"
#include <cstdio>
#include <vector>

SoundManager::SoundManager() {}

SoundManager::~SoundManager() {
    if (sfxTensar_) Mix_FreeChunk(sfxTensar_);
    if (sfxDisparo_) Mix_FreeChunk(sfxDisparo_);
    if (sfxImpacto_) Mix_FreeChunk(sfxImpacto_);
    if (sfxError_) Mix_FreeChunk(sfxError_);
    if (sfxDiana_) Mix_FreeChunk(sfxDiana_);
    if (sfxVictoria_) Mix_FreeChunk(sfxVictoria_);
    if (ready_) Mix_CloseAudio();
}

std::string SoundManager::resolvePath(const std::string& filename) {
    std::vector<std::string> candidates = {
        "games/sdl2/tiro_con_arco/assets/" + filename,
        "assets/" + filename,
        "../../games/sdl2/tiro_con_arco/assets/" + filename,
        "../games/sdl2/tiro_con_arco/assets/" + filename,
        filename
    };
    for (const auto& path : candidates) {
        FILE* f = std::fopen(path.c_str(), "rb");
        if (f) {
            std::fclose(f);
            return path;
        }
    }
    return filename;
}

bool SoundManager::init() {
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) < 0) {
        std::fprintf(stderr, "Aviso: Mix_OpenAudio fallo: %s\n", Mix_GetError());
        return false;
    }
    Mix_AllocateChannels(16);

    sfxTensar_ = Mix_LoadWAV(resolvePath("tensar.wav").c_str());
    sfxDisparo_ = Mix_LoadWAV(resolvePath("disparo.wav").c_str());
    sfxImpacto_ = Mix_LoadWAV(resolvePath("impacto.wav").c_str());
    sfxError_ = Mix_LoadWAV(resolvePath("error.wav").c_str()); // Opcional: punto de enganche para sonido de fallo.
    sfxDiana_ = Mix_LoadWAV(resolvePath("diana.wav").c_str());
    sfxVictoria_ = Mix_LoadWAV(resolvePath("victoria.wav").c_str());

    ready_ = true;
    return true;
}

void SoundManager::playDraw(float tension) {
    if (!ready_ || !sfxTensar_) return;
    if (drawChannel_ < 0 || !Mix_Playing(drawChannel_)) {
        drawChannel_ = Mix_PlayChannel(-1, sfxTensar_, 0);
    }
    if (drawChannel_ >= 0) {
        int vol = static_cast<int>(tension * 128.0f);
        Mix_Volume(drawChannel_, vol);
    }
}

void SoundManager::playShoot() {
    if (!ready_ || !sfxDisparo_) return;
    if (drawChannel_ >= 0) {
        Mix_HaltChannel(drawChannel_);
        drawChannel_ = -1;
    }
    Mix_PlayChannel(-1, sfxDisparo_, 0);
}

void SoundManager::playImpact() {
    if (!ready_ || !sfxImpacto_) return;
    Mix_PlayChannel(-1, sfxImpacto_, 0);
}

void SoundManager::playError() {
    if (!ready_) return;
    if (sfxError_) Mix_PlayChannel(-1, sfxError_, 0);
    else playImpact();
}

void SoundManager::playBullseye() {
    if (!ready_ || !sfxDiana_) return;
    Mix_PlayChannel(-1, sfxDiana_, 0);
}

void SoundManager::playFanfare() {
    if (!ready_ || !sfxVictoria_) return;
    Mix_PlayChannel(-1, sfxVictoria_, 0);
}
