#include "sound.h"
#include <cmath>
#include <cstdlib>
#include <vector>
#include <algorithm>

SoundManager::SoundManager() {}

SoundManager::~SoundManager() {
    if (sfxEngine_) Mix_FreeChunk(sfxEngine_);
    if (sfxScreech_) Mix_FreeChunk(sfxScreech_);
    if (sfxCheckpoint_) Mix_FreeChunk(sfxCheckpoint_);
    if (sfxCrash_) Mix_FreeChunk(sfxCrash_);
    if (ready_) Mix_CloseAudio();
}

Mix_Chunk* SoundManager::generateTone(int durationMs, int freq, float decay, float volume, int type) {
    int sampleRate = 44100;
    int numSamples = (sampleRate * durationMs) / 1000;
    int16_t* buffer = (int16_t*)std::malloc(numSamples * sizeof(int16_t));
    if (!buffer) return nullptr;

    for (int i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float progress = static_cast<float>(i) / numSamples;
        float env = std::exp(-progress * decay);
        float sample = 0.0f;

        if (type == 0) { // Motor V8 (diente de sierra + armónicos)
            float phase = std::fmod(t * freq, 1.0f);
            sample = (phase * 2.0f - 1.0f) * 0.6f + std::sin(2.0f * 3.14159f * freq * 0.5f * t) * 0.4f;
        } else if (type == 1) { // Chirrido de neumáticos (ruido filtrado)
            float noise = ((std::rand() % 2000) / 1000.0f) - 1.0f;
            sample = noise * (0.6f + 0.4f * std::sin(2.0f * 3.14159f * 1400.0f * t));
        } else if (type == 2) { // Campana Checkpoint (C5 + E5 + G5)
            sample = std::sin(2.0f * 3.14159f * 523.25f * t) * 0.4f +
                     std::sin(2.0f * 3.14159f * 659.25f * t) * 0.3f +
                     std::sin(2.0f * 3.14159f * 783.99f * t) * 0.3f;
        } else if (type == 3) { // Choque / impacto seco
            float noise = ((std::rand() % 2000) / 1000.0f) - 1.0f;
            float punch = std::sin(2.0f * 3.14159f * 70.0f * t);
            sample = noise * 0.6f + punch * 0.7f;
        }

        float finalVal = sample * env * volume * 32760.0f;
        finalVal = std::clamp(finalVal, -32760.0f, 32760.0f);
        buffer[i] = static_cast<int16_t>(finalVal);
    }

    Mix_Chunk* chunk = (Mix_Chunk*)std::malloc(sizeof(Mix_Chunk));
    if (!chunk) {
        std::free(buffer);
        return nullptr;
    }
    chunk->allocated = 1;
    chunk->abuf = (Uint8*)buffer;
    chunk->alen = numSamples * sizeof(int16_t);
    chunk->volume = MIX_MAX_VOLUME;
    return chunk;
}

bool SoundManager::init() {
    if (Mix_OpenAudio(44100, AUDIO_S16SYS, 1, 1024) < 0) {
        return false;
    }
    Mix_AllocateChannels(16);

    // Generar sonidos procedurales en memoria
    sfxEngine_ = generateTone(400, 110, 0.05f, 0.45f, 0);     // Bucle de motor
    sfxScreech_ = generateTone(500, 1200, 0.1f, 0.40f, 1);    // Chirrido derrape
    sfxCheckpoint_ = generateTone(800, 523, 3.5f, 0.65f, 2);  // Fanfarria checkpoint
    sfxCrash_ = generateTone(450, 70, 10.0f, 0.75f, 3);       // Impacto

    ready_ = true;
    return true;
}

void SoundManager::updateEngine(float rpm, float throttle) {
    if (!ready_ || !sfxEngine_) return;

    if (engineChannel_ < 0 || !Mix_Playing(engineChannel_)) {
        engineChannel_ = Mix_PlayChannel(-1, sfxEngine_, -1); // Bucle infinito
    }

    if (engineChannel_ >= 0) {
        float volFraction = 0.35f + throttle * 0.55f;
        Mix_Volume(engineChannel_, static_cast<int>(volFraction * 128.0f));
    }
}

void SoundManager::playScreech(bool active) {
    if (!ready_ || !sfxScreech_) return;
    if (active) {
        if (screechChannel_ < 0 || !Mix_Playing(screechChannel_)) {
            screechChannel_ = Mix_PlayChannel(-1, sfxScreech_, -1);
        }
    } else {
        if (screechChannel_ >= 0 && Mix_Playing(screechChannel_)) {
            Mix_HaltChannel(screechChannel_);
            screechChannel_ = -1;
        }
    }
}

void SoundManager::playCheckpoint() {
    if (!ready_ || !sfxCheckpoint_) return;
    Mix_PlayChannel(-1, sfxCheckpoint_, 0);
}

void SoundManager::playCrash() {
    if (!ready_ || !sfxCrash_) return;
    Mix_PlayChannel(-1, sfxCrash_, 0);
}
