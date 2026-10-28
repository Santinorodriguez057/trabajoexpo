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
    if (sfxShiftBeep_) Mix_FreeChunk(sfxShiftBeep_);
    if (ready_) Mix_CloseAudio();
}

Mix_Chunk* SoundManager::generateTone(int durationMs, int freq, float decay, float volume, int type) {
    int sampleRate = 44100;
    int numSamples = (sampleRate * durationMs) / 1000;
    // 2 canales estéreo de 16-bit (igual que MIX_DEFAULT_FORMAT)
    int totalSamples = numSamples * 2;
    int16_t* buffer = (int16_t*)std::malloc(totalSamples * sizeof(int16_t));
    if (!buffer) return nullptr;

    for (int i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float progress = static_cast<float>(i) / numSamples;
        float sample = 0.0f;

        if (type == 0) {
            // Sonido de motor deportivo / Muscle Car V8:
            // Graves pulsantes de combustión en cilindros (45 Hz a 90 Hz)
            float baseF = static_cast<float>(freq);
            // Asegurar empalme de bucle perfecto (ciclos completos exactos)
            float numCycles = std::round(baseF * (durationMs / 1000.0f));
            float actualF = numCycles / (durationMs / 1000.0f);

            float p1 = std::sin(2.0f * 3.14159265f * actualF * t);
            float p2 = std::sin(2.0f * 3.14159265f * actualF * 2.0f * t) * 0.5f;
            float p3 = std::sin(2.0f * 3.14159265f * actualF * 0.5f * t) * 0.7f; // Subgrave profundo
            float exhaustNoise = (((std::rand() % 1000) / 1000.0f) - 0.5f) * 0.15f;

            sample = (p1 * 0.5f + p2 + p3 + exhaustNoise) * 0.6f;

            // Ventana Hanning en los extremos para bucle sin clics ni chasquidos
            float fadeLength = 0.05f;
            float window = 1.0f;
            if (progress < fadeLength) window = progress / fadeLength;
            else if (progress > (1.0f - fadeLength)) window = (1.0f - progress) / fadeLength;
            sample *= window;

        } else if (type == 1) { // Chirrido de neumáticos (derrape)
            float noise = (((std::rand() % 2000) / 1000.0f) - 1.0f) * 0.5f;
            float squeal = std::sin(2.0f * 3.14159265f * 850.0f * t) * 0.35f;
            sample = (noise + squeal) * std::exp(-progress * decay);

        } else if (type == 2) { // Campana Checkpoint armónica
            float env = std::exp(-progress * decay);
            sample = (std::sin(2.0f * 3.14159265f * 523.25f * t) * 0.5f +
                      std::sin(2.0f * 3.14159265f * 659.25f * t) * 0.35f +
                      std::sin(2.0f * 3.14159265f * 783.99f * t) * 0.25f) * env;

        } else if (type == 3) { // Impacto / Choque contundente muy grave y pesado
            float env = std::exp(-progress * decay);
            float noise = (((std::rand() % 2000) / 1000.0f) - 1.0f) * 0.45f;
            // Sub-graves profundos y contundentes (38 Hz y 65 Hz)
            float subBass = std::sin(2.0f * 3.14159265f * 38.0f * t) * 0.90f;
            float punch = std::sin(2.0f * 3.14159265f * 65.0f * t) * 0.60f;
            sample = (noise + subBass + punch) * env;
        }

        float finalVal = std::clamp(sample * volume * 28000.0f, -32000.0f, 32000.0f);
        int16_t s16 = static_cast<int16_t>(finalVal);
        buffer[i * 2] = s16;     // Canal izquierdo
        buffer[i * 2 + 1] = s16; // Canal derecho
    }

    Mix_Chunk* chunk = (Mix_Chunk*)std::malloc(sizeof(Mix_Chunk));
    if (!chunk) {
        std::free(buffer);
        return nullptr;
    }
    chunk->allocated = 1;
    chunk->abuf = (Uint8*)buffer;
    chunk->alen = totalSamples * sizeof(int16_t);
    chunk->volume = static_cast<Uint8>(std::clamp(volume * 128.0f, 0.0f, 128.0f));
    return chunk;
}

bool SoundManager::init() {
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        return false;
    }
    Mix_AllocateChannels(16);

    // Generar sonidos con frecuencias graves naturales (55Hz para motor V8)
    sfxEngine_ = generateTone(300, 55, 0.0f, 0.50f, 0);       // Bucle de motor grave
    sfxScreech_ = generateTone(400, 850, 0.8f, 0.35f, 1);     // Chirrido derrape
    sfxCheckpoint_ = generateTone(700, 523, 3.2f, 0.60f, 2);  // Fanfarria checkpoint
    sfxCrash_ = generateTone(450, 55, 7.0f, 0.80f, 3);        // Impacto seco
    sfxShiftBeep_ = generateTone(120, 1050, 5.0f, 0.55f, 2);  // Beep agudo corto tipo F1/GT
    ready_ = true;
    return true;
}

void SoundManager::updateEngine(float rpm, float throttle) {
    if (!ready_ || !sfxEngine_) return;

    if (engineChannel_ < 0 || !Mix_Playing(engineChannel_)) {
        engineChannel_ = Mix_PlayChannel(-1, sfxEngine_, -1); // Bucle continuo
    }

    if (engineChannel_ >= 0) {
        // Modular volumen proporcionalmente a las RPM y acelerador
        float rpmNorm = std::clamp((rpm - 1000.0f) / 6000.0f, 0.0f, 1.0f);
        float volFraction = 0.25f + throttle * 0.35f + rpmNorm * 0.35f;
        Mix_Volume(engineChannel_, static_cast<int>(std::clamp(volFraction * 120.0f, 15.0f, 120.0f)));
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

void SoundManager::playShiftAlert() {
    if (!ready_ || !sfxShiftBeep_) return;
    Mix_PlayChannel(-1, sfxShiftBeep_, 0);
}
