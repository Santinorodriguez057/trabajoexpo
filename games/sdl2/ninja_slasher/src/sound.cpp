#include "sound.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>

namespace {
    constexpr int kSampleRate = 44100;
    constexpr float kTwoPi = 6.283185307f;

    // Convierte un buffer estéreo flotante [-1, 1] a Mix_Chunk 16-bit estéreo con soft-clipping
    Mix_Chunk* buildStereoChunk(const std::vector<float>& left, const std::vector<float>& right, float masterVol = 0.85f) {
        size_t n = std::min(left.size(), right.size());
        if (n == 0) return nullptr;

        int16_t* pcm = static_cast<int16_t*>(std::malloc(n * 2 * sizeof(int16_t)));
        if (!pcm) return nullptr;

        for (size_t i = 0; i < n; ++i) {
            // Soft clipper suave con tanh para sonido cálido y contundente sin saturación digital
            float l = std::tanh(left[i] * masterVol * 1.15f);
            float r = std::tanh(right[i] * masterVol * 1.15f);
            pcm[i * 2 + 0] = static_cast<int16_t>(std::clamp(l * 31500.0f, -32700.0f, 32700.0f));
            pcm[i * 2 + 1] = static_cast<int16_t>(std::clamp(r * 31500.0f, -32700.0f, 32700.0f));
        }

        Mix_Chunk* chunk = static_cast<Mix_Chunk*>(std::malloc(sizeof(Mix_Chunk)));
        if (!chunk) {
            std::free(pcm);
            return nullptr;
        }
        chunk->allocated = 1;
        chunk->abuf = reinterpret_cast<Uint8*>(pcm);
        chunk->alen = static_cast<Uint32>(n * 2 * sizeof(int16_t));
        chunk->volume = MIX_MAX_VOLUME;
        return chunk;
    }

    // Filtro pasa-bajos simple de un polo
    struct OnePoleLP {
        float z = 0.0f;
        float process(float in, float cutoffHz) {
            float rc = 1.0f / (kTwoPi * std::max(20.0f, cutoffHz));
            float dt = 1.0f / kSampleRate;
            float alpha = dt / (rc + dt);
            z += alpha * (in - z);
            return z;
        }
    };

    float whiteNoise() {
        return ((std::rand() & 0x7FFF) / 16383.5f) - 1.0f;
    }

    float sawWave(float phase) {
        float p = phase - std::floor(phase);
        return 2.0f * p - 1.0f;
    }

    // =========================================================================
    // Generadores de efectos específicos de alta calidad
    // =========================================================================

    // Silbido de aire de katana al cortar en vacío (Whoosh)
    Mix_Chunk* synthWhoosh(bool fiery) {
        int n = static_cast<int>(kSampleRate * (fiery ? 0.19f : 0.14f));
        std::vector<float> L(n, 0.0f), R(n, 0.0f);
        OnePoleLP lpL, lpR;
        float phase = 0.0f;

        for (int i = 0; i < n; ++i) {
            float u = static_cast<float>(i) / n;
            // Envolvente tipo campana asimétrica (ataque rápido, cola sedosa)
            float env = std::sin(u * 3.14159265f);
            env = std::pow(env, 0.75f) * std::exp(-u * 1.8f);

            // Barrido de filtro de aire + silbido fino de hoja
            float cutoff = fiery ? (500.0f + 3600.0f * std::sin(u * 3.14159f))
                                 : (650.0f + 4200.0f * std::pow(1.0f - u, 1.4f));
            float bladeFreq = (fiery ? 420.0f : 680.0f) + (fiery ? 520.0f : -360.0f) * u;
            phase += bladeFreq / kSampleRate;

            float airL = lpL.process(whiteNoise(), cutoff);
            float airR = lpR.process(whiteNoise(), cutoff * 1.06f);
            float whistle = std::sin(kTwoPi * phase) * 0.22f;

            float flameSub = fiery ? (std::sin(kTwoPi * (110.0f - 40.0f * u) * (static_cast<float>(i) / kSampleRate)) * 0.35f) : 0.0f;

            // Paneo dinámico sutil de izquierda a derecha
            float panL = 0.85f - 0.3f * u;
            float panR = 0.55f + 0.3f * u;
            L[i] = (airL * 1.35f + whistle + flameSub) * env * panL;
            R[i] = (airR * 1.35f + whistle + flameSub) * env * panR;
        }
        return buildStereoChunk(L, R, fiery ? 0.62f : 0.48f);
    }

    // Raspón metálico cuando se corta en mal ángulo (GRAZE)
    Mix_Chunk* synthGraze() {
        int n = static_cast<int>(kSampleRate * 0.16f);
        std::vector<float> L(n), R(n);
        OnePoleLP lp;
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / kSampleRate;
            float u = static_cast<float>(i) / n;
            float env = std::exp(-u * 7.5f);
            // Timbre inarmónico metálico (rebote en blindaje)
            float m1 = std::sin(kTwoPi * (540.0f - 110.0f * u) * t);
            float m2 = std::sin(kTwoPi * 863.0f * t) * 0.6f;
            float m3 = std::sin(kTwoPi * 1390.0f * t) * 0.35f * std::exp(-u * 14.0f);
            float scrape = lp.process(whiteNoise(), 2600.0f) * 0.55f * std::exp(-u * 10.0f);
            float s = (m1 * 0.5f + m2 + m3 + scrape) * env;
            L[i] = s * 0.95f;
            R[i] = s * 1.05f;
        }
        return buildStereoChunk(L, R, 0.68f);
    }

    // Corte limpio de katana (GOOD / PERFECT / SHURIKEN)
    Mix_Chunk* synthKatanaHit(int mode) {
        // mode: 0 = GOOD, 1 = PERFECT, 2 = SHURIKEN GOLD
        float dur = (mode == 1) ? 0.32f : ((mode == 2) ? 0.38f : 0.20f);
        int n = static_cast<int>(kSampleRate * dur);
        std::vector<float> L(n), R(n);
        OnePoleLP lpL, lpR;
        float swPhase = 0.0f;

        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / kSampleRate;
            float u = static_cast<float>(i) / n;

            // 1. Transiente de impacto ("punch" subgrave + click de filo)
            float punchFreq = 165.0f * std::exp(-t * 40.0f) + 54.0f;
            float punch = std::sin(kTwoPi * punchFreq * t) * std::exp(-t * 28.0f) * 0.85f;

            // 2. Rasgado de acero filtrado
            float slashEnv = std::exp(-t * 22.0f);
            float slashNoiseL = lpL.process(whiteNoise(), 4800.0f - 2800.0f * u) * slashEnv * 0.7f;
            float slashNoiseR = lpR.process(whiteNoise(), 5200.0f - 2800.0f * u) * slashEnv * 0.7f;

            // 3. Resonancia cristalina de la hoja (shing metálico)
            float ringL = 0.0f, ringR = 0.0f;
            if (mode == 0) {
                swPhase += (920.0f - 340.0f * u) / kSampleRate;
                float ring = std::sin(kTwoPi * swPhase) * std::exp(-t * 14.0f) * 0.45f;
                ringL = ring; ringR = ring * 0.95f;
            } else if (mode == 1) {
                // Acorde brillante de katana perfecta (Quinta + Octava + Armónico de acero)
                float envR = std::exp(-t * 8.5f);
                float f1 = 587.33f; // D5
                float f2 = 880.00f; // A5
                float f3 = 1174.66f; // D6
                float f4 = 2349.32f; // Brillo
                ringL = (std::sin(kTwoPi * f1 * t) * 0.45f +
                         std::sin(kTwoPi * f2 * t) * 0.35f +
                         std::sin(kTwoPi * f4 * t) * std::exp(-t * 18.0f) * 0.22f) * envR;
                ringR = (std::sin(kTwoPi * (f1 * 1.003f) * t) * 0.45f +
                         std::sin(kTwoPi * f3 * t) * 0.35f +
                         std::sin(kTwoPi * (f4 * 1.004f) * t) * std::exp(-t * 18.0f) * 0.22f) * envR;
            } else {
                // Shuriken dorado: doble tintineo metálico + acorde luminoso
                float envR = std::exp(-t * 7.0f);
                float bell1 = std::sin(kTwoPi * 1046.5f * t) * 0.42f;
                float bell2 = std::sin(kTwoPi * 1318.5f * t) * 0.36f;
                float bell3 = std::sin(kTwoPi * 1568.0f * t) * 0.32f;
                float shimmer = std::sin(kTwoPi * 2637.0f * t) * std::exp(-t * 13.0f) * 0.28f;
                ringL = (bell1 + bell3 + shimmer) * envR;
                ringR = (bell2 + bell3 * 1.02f + shimmer) * envR;
            }

            L[i] = punch + slashNoiseL + ringL;
            R[i] = punch + slashNoiseR + ringR;
        }
        return buildStereoChunk(L, R, (mode == 0) ? 0.72f : 0.85f);
    }

    // Killstreak estilo Valorant mejorado en estéreo con cuerpo grave y armónicos cálidos
    Mix_Chunk* synthKillstreakNote(float baseFreq, int tier) {
        int n = static_cast<int>(kSampleRate * 0.28f);
        std::vector<float> L(n), R(n);
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / kSampleRate;
            float u = static_cast<float>(i) / n;
            float attack = std::min(1.0f, t / 0.004f);

            // Golpe percusivo de impacto ("hitmarker thump")
            float punchFreq = 150.0f * std::exp(-t * 36.0f) + 48.0f;
            float punch = std::sin(kTwoPi * punchFreq * t) * std::exp(-t * 25.0f) * 0.70f;

            float decay = std::exp(-u * (4.2f - std::min(1.2f, tier * 0.14f)));
            float sub = std::sin(kTwoPi * (baseFreq * 0.5f) * t) * 0.42f;
            float rootL = std::sin(kTwoPi * baseFreq * t) * 0.52f;
            float rootR = std::sin(kTwoPi * (baseFreq * 1.004f) * t) * 0.52f; // Detune estéreo sutil
            float fifth = std::sin(kTwoPi * (baseFreq * 1.4983f) * t) * 0.34f;
            float oct = std::sin(kTwoPi * (baseFreq * 2.0f) * t) * std::exp(-u * 8.0f) * (0.18f + tier * 0.025f);

            L[i] = (punch + (sub + rootL + fifth * 0.85f + oct) * decay) * attack;
            R[i] = (punch + (sub + rootR + fifth * 1.15f + oct) * decay) * attack;
        }
        return buildStereoChunk(L, R, 0.78f);
    }

    // Fanfarria rápida de Multi-Corte (SLICE x2, x3, x4+)
    Mix_Chunk* synthMultiSlice(int level) {
        // Arpegio veloz ascendente cibernético
        int notesCount = 2 + level; // 3, 4 o 5 notas rápidas
        float dur = 0.26f + level * 0.06f;
        int n = static_cast<int>(kSampleRate * dur);
        std::vector<float> L(n, 0.0f), R(n, 0.0f);
        const float freqs[5] = {523.25f, 659.25f, 783.99f, 1046.50f, 1318.51f}; // C5 E5 G5 C6 E6

        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / kSampleRate;
            float u = static_cast<float>(i) / n;
            int step = std::min(notesCount - 1, static_cast<int>(u * notesCount));
            float localU = (u * notesCount) - step;
            float f = freqs[step];

            float noteEnv = std::min(1.0f, localU * 14.0f) * std::exp(-localU * 3.2f);
            float globalEnv = std::exp(-u * 1.4f);

            float sigL = (std::sin(kTwoPi * f * t) * 0.55f + sawWave(f * t) * 0.22f) * noteEnv * globalEnv;
            float sigR = (std::sin(kTwoPi * (f * 1.005f) * t) * 0.55f + sawWave(f * 0.5f * t) * 0.25f) * noteEnv * globalEnv;
            L[i] = sigL;
            R[i] = sigR;
        }
        return buildStereoChunk(L, R, 0.80f);
    }

    // Explosión PEM: caída sub-grave + crujido eléctrico + interferencia PEM
    Mix_Chunk* synthEmpExplosion() {
        int n = static_cast<int>(kSampleRate * 0.52f);
        std::vector<float> L(n), R(n);
        OnePoleLP lpL, lpR;
        float phaseSub = 0.0f, phaseZap = 0.0f;

        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / kSampleRate;
            float u = static_cast<float>(i) / n;

            // Sub-bass drop (110Hz -> 30Hz)
            float subFreq = 30.0f + 95.0f * std::exp(-t * 9.0f);
            phaseSub += subFreq / kSampleRate;
            float sub = std::sin(kTwoPi * phaseSub) * std::exp(-u * 3.0f) * 0.95f;

            // Descarga eléctrica PEM modulada en anillo (ring-mod zap)
            float zapFreq = 420.0f - 310.0f * u + 90.0f * std::sin(t * 95.0f);
            phaseZap += zapFreq / kSampleRate;
            float zap = std::sin(kTwoPi * phaseZap) * std::sin(kTwoPi * 58.0f * t) * std::exp(-u * 5.0f) * 0.55f;

            // Explosión de escombros filtrada
            float cutoff = 3800.0f * std::exp(-u * 4.5f) + 180.0f;
            float boomL = lpL.process(whiteNoise(), cutoff) * std::exp(-u * 4.2f) * 0.85f;
            float boomR = lpR.process(whiteNoise(), cutoff * 0.92f) * std::exp(-u * 4.2f) * 0.85f;

            L[i] = sub + zap + boomL;
            R[i] = sub - zap * 0.8f + boomR;
        }
        return buildStereoChunk(L, R, 0.92f);
    }

    // Penalización al dejar caer un objeto (bajada grave de escudo roto)
    Mix_Chunk* synthDropMiss() {
        int n = static_cast<int>(kSampleRate * 0.34f);
        std::vector<float> L(n), R(n);
        OnePoleLP lp;
        float ph = 0.0f;
        for (int i = 0; i < n; ++i) {
            float u = static_cast<float>(i) / n;
            float f = (u < 0.45f) ? 260.0f : 185.0f;
            f -= u * 45.0f;
            ph += f / kSampleRate;
            float env = std::exp(-u * 3.8f);
            float buzz = lp.process(sawWave(ph), 900.0f) * 0.7f + std::sin(kTwoPi * ph * 0.5f) * 0.5f;
            L[i] = buzz * env;
            R[i] = buzz * env * 0.95f;
        }
        return buildStereoChunk(L, R, 0.78f);
    }

    // Power-ups diferenciados: 0 = Hielo/Cristal, 1 = Doble Katana, 2 = Sobrecarga de Fuego
    Mix_Chunk* synthPowerup(int kind) {
        int n = static_cast<int>(kSampleRate * 0.48f);
        std::vector<float> L(n), R(n);
        OnePoleLP lp;
        float ph = 0.0f;
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / kSampleRate;
            float u = static_cast<float>(i) / n;
            float attack = std::min(1.0f, t / 0.008f);
            float env = std::exp(-u * 3.0f) * attack;

            if (kind == 0) {
                // CONGELACIÓN: barrido descendente de tiempo ralentizado + cristales de hielo agudos
                float slowFreq = 480.0f * std::exp(-u * 1.4f) + 90.0f;
                ph += slowFreq / kSampleRate;
                float iceChime = std::sin(kTwoPi * 1760.0f * t) * std::sin(kTwoPi * 14.0f * t) * 0.32f +
                                 std::sin(kTwoPi * 2349.3f * t) * std::exp(-u * 6.0f) * 0.28f;
                float sub = std::sin(kTwoPi * ph) * 0.55f;
                L[i] = (sub + iceChime) * env;
                R[i] = (sub + iceChime * 0.85f + std::sin(kTwoPi * 2093.0f * t) * 0.24f) * env;
            } else if (kind == 1) {
                // DOBLE FILO: desenvaine metálico doble en estéreo + acorde heroico
                float shing1 = std::sin(kTwoPi * (700.0f + 900.0f * u) * t) * std::exp(-u * 5.0f) * 0.35f;
                float shing2 = std::sin(kTwoPi * (880.0f + 1100.0f * u) * t) * std::exp(-std::max(0.0f, u - 0.15f) * 6.0f) * (u > 0.15f ? 0.38f : 0.0f);
                float chord = (std::sin(kTwoPi * 440.0f * t) + std::sin(kTwoPi * 659.25f * t) * 0.8f) * 0.35f;
                L[i] = (shing1 * 1.3f + shing2 * 0.5f + chord) * env;
                R[i] = (shing1 * 0.5f + shing2 * 1.3f + chord) * env;
            } else {
                // FRENESÍ X2: ignición ascendente + acorde de potencia sintetizado
                float rise = 220.0f * std::pow(2.0f, u * 1.5f);
                ph += rise / kSampleRate;
                float roar = lp.process(whiteNoise(), 600.0f + 2600.0f * u) * 0.40f;
                float lead = sawWave(ph) * 0.35f + std::sin(kTwoPi * 329.63f * t) * 0.35f + std::sin(kTwoPi * 493.88f * t) * 0.30f;
                L[i] = (roar + lead) * env;
                R[i] = (roar * 0.9f + lead * 1.08f) * env;
            }
        }
        return buildStereoChunk(L, R, 0.82f);
    }

    // Sonidos limpios de UI y cuenta regresiva
    Mix_Chunk* synthUiTone(float f1, float f2, float durSec, float vol) {
        int n = static_cast<int>(kSampleRate * durSec);
        std::vector<float> L(n), R(n);
        float ph = 0.0f;
        for (int i = 0; i < n; ++i) {
            float u = static_cast<float>(i) / n;
            float f = f1 + (f2 - f1) * u;
            ph += f / kSampleRate;
            float env = std::min(1.0f, u * 25.0f) * std::exp(-u * 5.5f);
            float s = std::sin(kTwoPi * ph) * 0.7f + std::sin(kTwoPi * ph * 2.0f) * 0.25f;
            L[i] = s * env;
            R[i] = s * env;
        }
        return buildStereoChunk(L, R, vol);
    }

    // Latido de alerta cuando queda 1 vida o últimos segundos
    Mix_Chunk* synthHeartbeat() {
        int n = static_cast<int>(kSampleRate * 0.32f);
        std::vector<float> L(n), R(n);
        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / kSampleRate;
            // Dos pulsos graves tipo "lub-dub"
            float p1 = std::exp(-t * 26.0f) * std::sin(kTwoPi * 62.0f * t);
            float t2 = std::max(0.0f, t - 0.13f);
            float p2 = (t > 0.13f) ? (std::exp(-t2 * 30.0f) * std::sin(kTwoPi * 78.0f * t2) * 0.85f) : 0.0f;
            float s = (p1 + p2) * 0.9f;
            L[i] = s;
            R[i] = s;
        }
        return buildStereoChunk(L, R, 0.65f);
    }

    // Fanfarria de fin de partida (Victoria/Récord vs Derrota)
    Mix_Chunk* synthGameOver(bool victory) {
        float dur = victory ? 0.85f : 0.70f;
        int n = static_cast<int>(kSampleRate * dur);
        std::vector<float> L(n), R(n);
        const float winNotes[4]  = {440.0f, 554.37f, 659.25f, 880.0f}; // A4 C#5 E5 A5
        const float loseNotes[3] = {329.63f, 293.66f, 220.0f};         // E4 D4 A3
        int count = victory ? 4 : 3;

        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / kSampleRate;
            float u = static_cast<float>(i) / n;
            int idx = std::min(count - 1, static_cast<int>(u * count));
            float lu = (u * count) - idx;
            float f = victory ? winNotes[idx] : loseNotes[idx];
            float env = std::min(1.0f, lu * 18.0f) * std::exp(-lu * (idx == count - 1 ? 1.8f : 3.5f));
            float body = std::sin(kTwoPi * f * t) * 0.55f +
                         std::sin(kTwoPi * (f * 0.5f) * t) * 0.35f +
                         sawWave(f * t) * 0.18f;
            L[i] = body * env;
            R[i] = body * env * 0.98f;
        }
        return buildStereoChunk(L, R, 0.78f);
    }

    // =========================================================================
    // Sintetizador procedural de Música Cyberpunk / Synthwave perfectamente loopeable
    // =========================================================================
    Mix_Chunk* synthCyberpunkBgm(bool energetic) {
        // 128 BPM -> 1 negra = 0.46875s, 1 semicorchea = 0.1171875s
        // 32 pasos (2 compases completos) = 3.75 segundos exactos -> loop sin costuras
        const int steps = 32;
        const float stepDur = energetic ? 0.115f : 0.135f; // ~130 BPM en partida, ~111 BPM en menú
        const float totalDur = steps * stepDur;
        const int n = static_cast<int>(std::round(totalDur * kSampleRate));

        std::vector<float> L(n, 0.0f), R(n, 0.0f);
        OnePoleLP bassLp, leadLpL, leadLpR, hatLp;

        // Progresión oscura cyberpunk en La menor (Am - F - G - Em) -> 8 pasos por acorde
        const float bassRoots[4] = {55.0f, 43.65f, 49.0f, 41.20f}; // A1, F1, G1, E1
        // Arpegio hipnótico menor
        const float arpOffsets[8] = {4.0f, 4.7568f, 5.9932f, 8.0f, 5.9932f, 4.7568f, 7.127f, 5.9932f};

        float bassPhase = 0.0f;
        float arpPhaseL = 0.0f, arpPhaseR = 0.0f;

        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / kSampleRate;
            float exactStep = t / stepDur;
            int step = static_cast<int>(exactStep) % steps;
            float stepFrac = exactStep - std::floor(exactStep);
            float stepTime = stepFrac * stepDur;

            int chordIdx = (step / 8) % 4;
            float root = bassRoots[chordIdx];

            // 1. Bajo Synthwave pulsante en semicorcheas (rolling bassline)
            float bassFreq = root * ((step % 2 == 1 && energetic) ? 2.0f : 1.0f);
            bassPhase += bassFreq / kSampleRate;
            float bassEnv = std::min(1.0f, stepFrac * 18.0f) * std::exp(-stepFrac * (energetic ? 2.8f : 3.6f));
            float rawBass = sawWave(bassPhase) * 0.65f + std::sin(kTwoPi * bassPhase) * 0.55f;
            float cutoffBass = (energetic ? 520.0f : 340.0f) + 650.0f * std::exp(-stepFrac * 5.0f);
            float bassSig = bassLp.process(rawBass, cutoffBass) * bassEnv * 0.52f;

            // 2. Bombo electrónico (Kick en pasos 0, 4, 8, 12...) solo en partida o suave en menú
            float kick = 0.0f;
            if (step % 4 == 0) {
                float kFreq = 46.0f + 115.0f * std::exp(-stepTime * 38.0f);
                kick = std::sin(kTwoPi * kFreq * stepTime) * std::exp(-stepTime * 15.0f) * (energetic ? 0.62f : 0.32f);
            }

            // 3. Caja / Hi-hat electrónico sutil en contratiempo (solo modo energético)
            float percL = 0.0f, percR = 0.0f;
            if (energetic) {
                if (step % 8 == 4) {
                    // Snare cyber corto
                    float sn = hatLp.process(whiteNoise(), 2800.0f) * std::exp(-stepTime * 24.0f) * 0.28f;
                    float snBody = std::sin(kTwoPi * 185.0f * stepTime) * std::exp(-stepTime * 30.0f) * 0.22f;
                    percL += sn + snBody;
                    percR += sn * 0.95f + snBody;
                } else if (step % 2 == 1) {
                    // Off-beat hi-hat fino
                    float hh = (whiteNoise() - hatLp.process(whiteNoise(), 5500.0f)) * std::exp(-stepTime * 45.0f) * 0.14f;
                    percL += hh * 0.8f;
                    percR += hh * 1.1f;
                }
            }

            // 4. Arpegio Pluck Synthwave en estéreo
            float arpMult = arpOffsets[step % 8];
            float arpFreq = root * arpMult;
            arpPhaseL += arpFreq / kSampleRate;
            arpPhaseR += (arpFreq * 1.004f) / kSampleRate;
            float arpEnv = std::min(1.0f, stepFrac * 22.0f) * std::exp(-stepFrac * (energetic ? 4.2f : 5.5f));
            float arpGain = energetic ? 0.20f : 0.15f;
            float arpL = leadLpL.process(sawWave(arpPhaseL), 1600.0f + 900.0f * arpEnv) * arpEnv * arpGain;
            float arpR = leadLpR.process(sawWave(arpPhaseR), 1800.0f + 900.0f * arpEnv) * arpEnv * arpGain;

            // Crossfade microscópico en los primeros/últimos 128 samples para cero click de bucle
            float edge = 1.0f;
            if (i < 128) edge = i / 128.0f;
            else if (i > n - 128) edge = (n - i) / 128.0f;

            L[i] = (bassSig + kick + percL + arpL * 1.15f) * edge;
            R[i] = (bassSig + kick + percR + arpR * 1.15f) * edge;
        }

        return buildStereoChunk(L, R, energetic ? 0.36f : 0.28f);
    }
}

SoundSystem::SoundSystem() = default;

SoundSystem::~SoundSystem() {
    shutdown();
}

bool SoundSystem::init() {
    if (ready_) return true;

    if (Mix_OpenAudio(kSampleRate, AUDIO_S16SYS, 2, 1024) < 0) {
        return false;
    }
    Mix_AllocateChannels(16);
    // Canal 0: BGM | Canales 1 y 2: Killstreak P1 y P2 | Canal 3: Powerup / MultiSlice | Canales 4..5: Whoosh P1/P2
    Mix_ReserveChannels(6);

    bgmMenu_        = synthCyberpunkBgm(false);
    bgmGame_        = synthCyberpunkBgm(true);

    sfxUiNav_       = synthUiTone(540.0f, 720.0f, 0.065f, 0.45f);
    sfxUiConfirm_   = synthUiTone(523.25f, 1046.5f, 0.16f, 0.65f);
    sfxCountBeep_   = synthUiTone(659.25f, 659.25f, 0.11f, 0.60f);
    sfxCountGo_     = synthUiTone(880.0f, 1318.5f, 0.24f, 0.78f);

    sfxWhoosh_      = synthWhoosh(false);
    sfxWhooshFire_  = synthWhoosh(true);
    sfxGraze_       = synthGraze();
    sfxSliceGood_   = synthKatanaHit(0);
    sfxSlicePerfect_= synthKatanaHit(1);
    sfxShurikenHit_ = synthKatanaHit(2);

    for (int m = 0; m < 3; ++m) {
        sfxMultiSlice_[m] = synthMultiSlice(m + 1);
    }

    sfxExplode_     = synthEmpExplosion();
    sfxDropMiss_    = synthDropMiss();
    sfxPowerFreeze_ = synthPowerup(0);
    sfxPowerDual_   = synthPowerup(1);
    sfxPowerFrenzy_ = synthPowerup(2);
    sfxHeartbeat_   = synthHeartbeat();
    sfxGameOverLose_= synthGameOver(false);
    sfxGameOverWin_ = synthGameOver(true);

    // Escala pentatónica menor ascendente (A3 -> C4 -> D4 -> E4 -> G4 -> A4 -> C5 -> E5)
    const float streakFreqs[8] = {220.0f, 261.63f, 293.66f, 329.63f, 392.00f, 440.00f, 523.25f, 659.25f};
    for (int k = 0; k < 8; ++k) {
        sfxKillstreak_[k] = synthKillstreakNote(streakFreqs[k], k);
    }

    ready_ = true;
    return true;
}

void SoundSystem::shutdown() {
    if (!ready_) return;
    Mix_HaltChannel(-1);

    auto freeChunk = [](Mix_Chunk*& c) {
        if (c) { Mix_FreeChunk(c); c = nullptr; }
    };

    freeChunk(bgmMenu_);
    freeChunk(bgmGame_);
    freeChunk(sfxUiNav_);
    freeChunk(sfxUiConfirm_);
    freeChunk(sfxCountBeep_);
    freeChunk(sfxCountGo_);
    freeChunk(sfxWhoosh_);
    freeChunk(sfxWhooshFire_);
    freeChunk(sfxGraze_);
    freeChunk(sfxSliceGood_);
    freeChunk(sfxSlicePerfect_);
    freeChunk(sfxShurikenHit_);
    for (auto& c : sfxMultiSlice_) freeChunk(c);
    freeChunk(sfxExplode_);
    freeChunk(sfxDropMiss_);
    freeChunk(sfxPowerFreeze_);
    freeChunk(sfxPowerDual_);
    freeChunk(sfxPowerFrenzy_);
    freeChunk(sfxHeartbeat_);
    freeChunk(sfxGameOverLose_);
    freeChunk(sfxGameOverWin_);
    for (auto& c : sfxKillstreak_) freeChunk(c);

    Mix_CloseAudio();
    ready_ = false;
}

void SoundSystem::startMenuBgm() {
    if (!ready_ || !bgmMenu_ || currentBgmMode_ == 1) return;
    Mix_HaltChannel(bgmChannel_);
    Mix_Volume(bgmChannel_, 96);
    Mix_PlayChannel(bgmChannel_, bgmMenu_, -1);
    currentBgmMode_ = 1;
}

void SoundSystem::startGameBgm() {
    if (!ready_ || !bgmGame_ || currentBgmMode_ == 2) return;
    Mix_HaltChannel(bgmChannel_);
    Mix_Volume(bgmChannel_, 108);
    Mix_PlayChannel(bgmChannel_, bgmGame_, -1);
    currentBgmMode_ = 2;
}

void SoundSystem::setBgmDuck(bool slowMoOrFreeze) {
    if (!ready_) return;
    int targetVol = slowMoOrFreeze ? 46 : (currentBgmMode_ == 2 ? 108 : 96);
    Mix_Volume(bgmChannel_, targetVol);
}

void SoundSystem::playUiNav() {
    if (ready_ && sfxUiNav_) Mix_PlayChannel(-1, sfxUiNav_, 0);
}

void SoundSystem::playUiConfirm() {
    if (ready_ && sfxUiConfirm_) Mix_PlayChannel(-1, sfxUiConfirm_, 0);
}

void SoundSystem::playCountdownBeep(bool isGo) {
    if (!ready_) return;
    Mix_Chunk* c = isGo ? sfxCountGo_ : sfxCountBeep_;
    if (c) Mix_PlayChannel(-1, c, 0);
}

void SoundSystem::playGameOver(bool victoryOrRecord) {
    if (!ready_) return;
    Mix_Chunk* c = victoryOrRecord ? sfxGameOverWin_ : sfxGameOverLose_;
    if (c) Mix_PlayChannel(-1, c, 0);
}

void SoundSystem::playWhoosh(int playerIndex, bool isFrenzy) {
    if (!ready_) return;
    int ch = 4 + (playerIndex == 0 ? 0 : 1);
    if (Mix_Playing(ch)) return; // Evita solapar ráfagas continuas del mismo dedo
    Mix_Chunk* c = isFrenzy ? sfxWhooshFire_ : sfxWhoosh_;
    if (c) Mix_PlayChannel(ch, c, 0);
}

void SoundSystem::playSlice(int rating, bool isShuriken) {
    if (!ready_) return;
    Mix_Chunk* c = sfxSliceGood_;
    if (isShuriken) c = sfxShurikenHit_;
    else if (rating >= 3) c = sfxSlicePerfect_;
    else if (rating <= 1) c = sfxGraze_;
    if (c) Mix_PlayChannel(-1, c, 0);
}

void SoundSystem::playKillstreak(int playerIndex, int combo) {
    if (!ready_ || combo < 2) return;
    int idx = std::clamp(combo - 2, 0, 7);
    if (sfxKillstreak_[idx]) {
        Mix_PlayChannel(1 + (playerIndex == 0 ? 0 : 1), sfxKillstreak_[idx], 0);
    }
}

void SoundSystem::playMultiSlice(int count) {
    if (!ready_ || count < 2) return;
    int idx = std::clamp(count - 2, 0, 2);
    if (sfxMultiSlice_[idx]) {
        Mix_PlayChannel(3, sfxMultiSlice_[idx], 0);
    }
}

void SoundSystem::playMineExplosion() {
    if (ready_ && sfxExplode_) Mix_PlayChannel(-1, sfxExplode_, 0);
}

void SoundSystem::playDropMiss() {
    if (ready_ && sfxDropMiss_) Mix_PlayChannel(-1, sfxDropMiss_, 0);
}

void SoundSystem::playPowerup(int powerupKind) {
    if (!ready_) return;
    Mix_Chunk* c = sfxPowerFreeze_;
    if (powerupKind == 1) c = sfxPowerDual_;
    else if (powerupKind == 2) c = sfxPowerFrenzy_;
    if (c) Mix_PlayChannel(3, c, 0);
}

void SoundSystem::playDangerHeartbeat() {
    if (ready_ && sfxHeartbeat_) Mix_PlayChannel(-1, sfxHeartbeat_, 0);
}
