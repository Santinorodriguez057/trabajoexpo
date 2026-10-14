#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cstdint>

// Generador procedural de SFX (WAV 16-bit mono 44100Hz en memoria)
static Mix_Chunk* generateToneChunk(int durationMs, int freq, float decay = 5.0f, float volume = 0.5f, int type = 0) {
    int sampleRate = 44100;
    int numSamples = (sampleRate * durationMs) / 1000;
    int16_t* buffer = (int16_t*)std::malloc(numSamples * sizeof(int16_t));
    if (!buffer) return nullptr;

    for (int i = 0; i < numSamples; ++i) {
        float t = (float)i / (float)sampleRate;
        float progress = (float)i / (float)numSamples;
        float env = std::exp(-progress * decay);
        float sample = 0.0f;

        if (type == 0) { // Seno puro / Sub-bass punch para latido cardíaco
            sample = std::sin(2.0f * 3.14159f * (float)freq * t);
            if (freq < 100) {
                sample += 0.5f * std::sin(2.0f * 3.14159f * ((float)freq * 0.5f) * t);
            }
        } else if (type == 1) { // Ruido / Chasquido mecánico (Tic-Tac / Martillo del revólver)
            float noise = ((float)(std::rand() % 2000) / 1000.0f) - 1.0f;
            sample = noise * 0.7f + 0.3f * std::sin(2.0f * 3.14159f * (float)freq * t);
        } else if (type == 2) { // Campana western lejana (Armónicos ricos)
            sample = 0.5f * std::sin(2.0f * 3.14159f * (float)freq * t) +
                     0.3f * std::sin(2.0f * 3.14159f * (float)freq * 2.76f * t) +
                     0.2f * std::sin(2.0f * 3.14159f * (float)freq * 5.4f * t);
        } else if (type == 3) { // Silbido del viento
            float noise = ((float)(std::rand() % 2000) / 1000.0f) - 1.0f;
            sample = noise * (0.3f + 0.2f * std::sin(t * 3.0f));
        } else if (type == 4) { // Sub-drop time warp / cámara lenta cinematográfica
            float curFreq = (float)freq * (1.0f - progress * 0.72f);
            sample = std::sin(2.0f * 3.14159f * curFreq * t) + 
                     0.45f * std::sin(2.0f * 3.14159f * (curFreq * 0.5f) * t);
        } else if (type == 5) { // Silbido balístico supersónico en cámara lenta con efecto Doppler
            float tremolo = 0.65f + 0.35f * std::sin(2.0f * 3.14159f * 18.0f * t);
            float curFreq = (float)freq * (1.0f - progress * 0.28f);
            sample = std::sin(2.0f * 3.14159f * curFreq * t) * tremolo;
        } else if (type == 6) { // Impacto demoledor / sombrerazo seco y contundente
            float noise = ((float)(std::rand() % 2000) / 1000.0f) - 1.0f;
            float punch = std::sin(2.0f * 3.14159f * 65.0f * t);
            sample = noise * 0.55f * std::exp(-progress * 18.0f) + punch * 0.75f;
        }

        float finalVal = sample * env * volume * 32760.0f;
        if (finalVal > 32760.0f) finalVal = 32760.0f;
        if (finalVal < -32760.0f) finalVal = -32760.0f;
        buffer[i] = (int16_t)finalVal;
    }

    Mix_Chunk* chunk = (Mix_Chunk*)std::malloc(sizeof(Mix_Chunk));
    if (!chunk) { std::free(buffer); return nullptr; }
    chunk->allocated = 1;
    chunk->abuf = (Uint8*)buffer;
    chunk->alen = numSamples * sizeof(int16_t);
    chunk->volume = (Uint8)(volume * 128);
    return chunk;
}

// Estados del juego
enum GameState {
    STATE_WAITING, 
    STATE_TENSION, 
    STATE_DRAW,    
    STATE_SHOWDOWN, // Secuencia cinemática (Hit-stop inverso -> Bullet time -> Caída)
    STATE_RESULT   
};

struct Player {
    SDL_GameController* gc = nullptr;
    bool ready = false;
    bool foul = false;
    bool has_drawn = false;
    bool has_gyro = false;
    bool has_accel = false;
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

static std::unordered_map<std::string, SDL_Texture*> g_textCache;

void renderText(SDL_Renderer* ren, TTF_Font* font, const std::string& text, int x, int y, SDL_Color color, bool center = false, float scale = 1.0f) {
    if (!font || text.empty()) return;
    std::string key = text + "_" + std::to_string(color.r) + "_" + std::to_string(color.g) + "_" + std::to_string(color.b);
    SDL_Texture* tex = nullptr;
    auto it = g_textCache.find(key);
    if (it == g_textCache.end()) {
        SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text.c_str(), color);
        if (surf) {
            tex = SDL_CreateTextureFromSurface(ren, surf);
            SDL_FreeSurface(surf);
            if (tex) g_textCache[key] = tex;
        }
    } else {
        tex = it->second;
    }
    if (!tex) return;

    int w, h;
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    
    int dstW = static_cast<int>(w * scale);
    int dstH = static_cast<int>(h * scale);
    int dstX = center ? (x - dstW / 2) : x;
    int dstY = center ? (y - dstH / 2) : y;
    
    SDL_Rect dst = {dstX, dstY, dstW, dstH};
    SDL_RenderCopy(ren, tex, nullptr, &dst);
}

void drawCowboy(SDL_Renderer* ren, SDL_Texture* whiteTex, float cx, float cy, int state_phase, bool flip, float t, float tension_prog = 0.0f, bool showGroundHat = true) {
    const float S = 1.35f; // Escala imponente original
    
    // Desfase único por jugador (usando 'flip') para que P1 y P2 no respiren ni parpadeen como robots clonados
    float phaseOffset = flip ? 2.15f : 0.0f;
    float tLocal = t + phaseOffset;

    // Respiración dinámica según el estado emocional
    // En fase 2 (disparo/victoria) se mantiene firme y plantado, sin subir y bajar acelerado
    float breatheFreq = (state_phase == 1) ? (6.0f + tension_prog * 6.0f) : ((state_phase == 2) ? 0.8f : 3.2f);
    float breatheAmp  = (state_phase == 1) ? 2.5f : ((state_phase == 2) ? 0.6f : 4.0f);
    float breathe     = std::sin(tLocal * breatheFreq) * breatheAmp;
    float chestExpand = (state_phase == 2) ? 0.8f : (std::max(0.0f, std::sin(tLocal * breatheFreq)) * 2.5f);

    // Vibración muscular en tensión
    float shake = (state_phase == 1) ? (std::sin(t * (55.0f + tension_prog * 35.0f)) * (1.5f + tension_prog * 7.5f)) : 0.0f;

    // Simulación de viento del desierto (costo cero en CPU)
    float windWave  = std::sin(t * 4.5f + (flip ? 1.0f : 0.0f)) * 6.0f + std::cos(t * 7.2f) * 3.0f;
    float windGust  = (state_phase == 1) ? (windWave * (1.0f + tension_prog * 0.8f)) : windWave;

    // Parpadeo natural cada ~3.5 segundos (solo en espera o falta)
    float blinkCycle = std::fmod(tLocal, 3.6f);
    bool isBlinking  = (state_phase == 0 || state_phase == 4) && (blinkCycle > 3.42f && blinkCycle < 3.56f);

    SDL_RendererFlip flipFlag = flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

    // Helper ultra-rápido (misma arquitectura 1x1 texture batching)
    auto drawR = [&](float lx, float ly, float w, float h, float angle, SDL_Color fill, bool outline = true, int borderW = 3, SDL_Color borderCol = {18, 18, 18, 255}) {
        float sw = w * S;
        float sh = h * S;
        float fx = cx + (flip ? -lx * S : lx * S) - sw / 2.0f;
        float fy = cy + ly * S - sh / 2.0f;
        float fa = flip ? -angle : angle;
        SDL_Point center = { (int)(sw / 2.0f), (int)(sh / 2.0f) };

        if (outline && borderW > 0) {
            int bW = static_cast<int>(borderW * 1.25f);
            SDL_SetTextureColorMod(whiteTex, borderCol.r, borderCol.g, borderCol.b);
            SDL_Rect oR = { (int)(fx - bW), (int)(fy - bW), (int)(sw + bW * 2), (int)(sh + bW * 2) };
            SDL_Point oCenter = { (int)((sw + bW * 2) / 2.0f), (int)((sh + bW * 2) / 2.0f) };
            SDL_RenderCopyEx(ren, whiteTex, nullptr, &oR, fa, &oCenter, flipFlag);
        }

        SDL_SetTextureColorMod(whiteTex, fill.r, fill.g, fill.b);
        SDL_Rect cR = { (int)fx, (int)fy, (int)sw, (int)sh };
        SDL_RenderCopyEx(ren, whiteTex, nullptr, &cR, fa, &center, flipFlag);
    };

    // Paleta Neo-Brutalista enriquecida con acentos cálidos/dramáticos
    SDL_Color cWhite      = {255, 255, 255, 255};
    SDL_Color cBackLimb   = {215, 215, 220, 255};
    SDL_Color cPoncho     = flip ? SDL_Color{60, 52, 50, 255} : SDL_Color{50, 56, 64, 255}; // Sutil distinción J1 vs J2
    SDL_Color cPonchoTrim = {210, 180, 95, 255};
    SDL_Color cBandana    = flip ? SDL_Color{150, 35, 35, 255} : SDL_Color{40, 40, 45, 255}; // J1 Negro carbón, J2 Rojo forajido
    SDL_Color cHatBand    = {35, 35, 38, 255};
    SDL_Color cFaceShadow = {42, 42, 48, 255};
    SDL_Color cBlack      = {18, 18, 18, 255};
    SDL_Color cSilver     = {235, 235, 240, 255};
    SDL_Color cGold       = {245, 205, 45, 255};
    SDL_Color cHolster    = {52, 48, 46, 255};
    SDL_Color cFlash      = {255, 235, 70, 255};
    SDL_Color cRedGlow    = {255, 50, 50, 255};
    SDL_Color cSweat      = {120, 210, 255, 255};

    // =========================================================================
    // CASO 1: VAQUERO DERROTADO (Fase 3) - K.O. Cinematográfico
    // =========================================================================
    if (state_phase == 3) {
        cWhite = {200, 200, 205, 255};
        cBackLimb = {165, 165, 170, 255};
        float koPulse = std::sin(t * 2.0f) * 1.5f; // Leve respiración en el suelo

        // Sombra alargada en el suelo
        drawR(0, 155, 235, 16, 0.0f, cBlack, false);

        // Capa/Poncho desparramado en el piso
        drawR(-25, 148, 95, 18, -4.0f, cPoncho, true, 2);

        // Piernas estiradas (una ligeramente flexionada por el impacto)
        drawR(48, 134, 80, 24, -6.0f, cBackLimb, true, 3);
        drawR(45, 142, 85, 26, 2.0f, cWhite, true, 3);
        
        // Botas caídas con espuelas doradas
        drawR(90, 138, 38, 24, 8.0f, cWhite, true, 3);
        drawR(92, 148, 40, 5, 8.0f, cBlack, false);
        drawR(108, 146, 10, 10, t * 40.0f, cGold, true, 1); // Espuela girando lento

        // Torso plano en el suelo con levísimo movimiento
        drawR(-20, 136 + koPulse * 0.4f, 76, 46, 0.0f, cWhite, true, 3);
        // Estrella de Sheriff caída en el pecho
        drawR(-28, 120 + koPulse * 0.4f, 12, 12, 15.0f, cGold, true, 1);

        // Cinturón y hebilla
        drawR(12, 136, 16, 48, 0.0f, cBlack, true, 2);
        drawR(12, 136, 16, 18, 0.0f, cSilver, true, 2);

        // Pañuelo
        drawR(-52, 135, 22, 32, 8.0f, cBandana, true, 2);

        // Cabeza en el suelo
        drawR(-78, 134, 46, 44, -5.0f, cWhite, true, 3);
        // Ojos de "X" de derrota
        drawR(-76, 130, 13, 3, 45.0f, cBlack, false);
        drawR(-76, 130, 13, 3, -45.0f, cBlack, false);
        // Lengua afuera cómica / palillo roto
        drawR(-64, 142, 14, 3, -30.0f, cGold, false);

        // Estrellitas de K.O. orbitando sobre la cabeza caída (efecto 3D falso súper ligero)
        for (int i = 0; i < 3; ++i) {
            float ang = t * 4.5f + i * 2.094f; // 120 grados de separación
            float sx = -78.0f + std::cos(ang) * 32.0f;
            float sy = 96.0f + std::sin(ang) * 9.0f;
            drawR(sx, sy, 9, 9, t * 180.0f, cGold, true, 1);
            drawR(sx, sy, 9, 9, t * 180.0f + 45.0f, cGold, false);
        }

        // Sombrero volado a un costado mecido por el viento
        if (showGroundHat) {
            float hatRock = std::sin(t * 3.0f) * 4.0f;
            drawR(-124, 142, 135, 12, 20.0f + hatRock, cWhite, true, 3);
            drawR(-120, 124, 50, 34, 20.0f + hatRock, cWhite, true, 3);
            drawR(-122, 136, 50, 12, 20.0f + hatRock, cHatBand, true, 2);
        }

        // Revólver caído en el suelo largando un hilito de humo
        drawR(118, 146, 38, 14, -15.0f, cWhite, true, 2);
        drawR(104, 150, 14, 22, 25.0f, cWhite, true, 2);
        float smokeRise = std::fmod(t * 25.0f, 35.0f);
        drawR(138 + std::sin(t * 5.0f) * 4.0f, 135 - smokeRise, 10 - smokeRise * 0.2f, 10 - smokeRise * 0.2f, t * 45.0f, {200, 200, 200, 255}, false);
        return;
    }

    // =========================================================================
    // VAQUERO EN PIE (Fases 0, 1, 2, 4)
    // =========================================================================
    float bodyAngle = 0.0f;
    float headTilt = 0.0f;
    float kneeFlex = 0.0f;

    if (state_phase == 0) { // Espera con actitud
        bodyAngle = std::sin(tLocal * 1.6f) * 1.2f;
        headTilt = std::sin(tLocal * 1.6f - 0.5f) * 1.5f;
    } else if (state_phase == 1) { // Tensión (agazapado y listo para matar)
        bodyAngle = 7.5f + tension_prog * 4.0f;
        headTilt = 4.0f + tension_prog * 3.0f; // Baja el ala del sombrero intimidante
        kneeFlex = 4.0f + tension_prog * 3.0f;
    } else if (state_phase == 2) { // Disparo / Quick-draw
        bodyAngle = -6.0f; // Inclinación heroica hacia atrás absorbiendo el calibre .45
        headTilt = 2.0f;
    } else if (state_phase == 4) { // Falta (avergonzado / cabizbajo)
        bodyAngle = 14.0f;
        headTilt = 22.0f;
    }

    float torsoY = -5.0f + breathe + shake + (kneeFlex * 0.6f);
    float headY  = -86.0f + breathe * 1.15f + shake + (kneeFlex * 0.6f);

    // 1. SOMBRA DE SUELO DINÁMICA
    drawR(2, 155, 145 + chestExpand * 2.0f, 14, 0.0f, cBlack, false);

    // 2. PONCHO / GUARDAPOLVO TRASERO (Onda de tela al viento detrás de las piernas)
    float capeAngle = bodyAngle + 12.0f + windGust * 0.9f + ((state_phase == 2) ? 18.0f : 0.0f);
    drawR(-28 - windGust * 0.4f, torsoY + 38, 34, 88, capeAngle, cPoncho, true, 3);
    drawR(-34 - windGust * 0.6f, torsoY + 76, 30, 6, capeAngle, cPonchoTrim, false); // Franja decorativa del poncho

    // 3. BRAZO TRASERO
    if (state_phase == 2) {
        drawR(-20, torsoY - 4, 18, 44, bodyAngle + 24.0f, cBackLimb, true, 3);
        drawR(-14, torsoY + 30, 16, 42, bodyAngle - 32.0f, cBackLimb, true, 3);
        drawR(-8, torsoY + 52, 16, 16, bodyAngle - 10.0f, cBackLimb, true, 2);
    } else if (state_phase == 1) {
        drawR(-30, torsoY - 6, 18, 46, bodyAngle - 24.0f, cBackLimb, true, 3);
        drawR(-38, torsoY + 30, 16, 42, bodyAngle - 8.0f, cBackLimb, true, 3);
        drawR(-38, torsoY + 58, 16, 16, bodyAngle - 8.0f, cBackLimb, true, 2);
    } else {
        float idleSwing = std::sin(tLocal * 1.6f) * 3.0f;
        drawR(-34, torsoY + 4, 18, 46, bodyAngle - 10.0f + idleSwing, cBackLimb, true, 3);
        drawR(-38, torsoY + 42, 16, 46, bodyAngle + 2.0f + idleSwing, cBackLimb, true, 3);
        drawR(-38, torsoY + 62, 16, 4, bodyAngle + 2.0f + idleSwing, cBlack, false);
        drawR(-38, torsoY + 74, 16, 18, bodyAngle + 2.0f + idleSwing, cBackLimb, true, 2);
    }

    // 4. PIERNAS, CHAPARRERAS Y BOTAS CON ESPUELAS
    // Pierna trasera
    drawR(-24, 95, 34, 96, -3.0f - kneeFlex * 0.5f, cBackLimb, true, 3);
    drawR(-24, 95, 24, 3, -3.0f, cBlack, false);
    drawR(-28, 145, 48, 20, 0.0f, cBackLimb, true, 3);
    drawR(-46, 149, 12, 10, 0.0f, cBlack, false);
    drawR(-54, 147, 9, 9, t * 30.0f, cGold, true, 1); // Espuela trasera
    drawR(-26, 153, 52, 4, 0.0f, cBlack, false);

    // Pierna delantera
    drawR(24, 95, 36, 96, 4.0f + kneeFlex * 0.5f, cWhite, true, 3);
    drawR(10, 95, 10, 92, 4.0f + kneeFlex * 0.5f, cBackLimb, false);
    // Flecos laterales de las chaparreras (Chaps) que vibran con el viento
    for (int f = 0; f < 4; ++f) {
        drawR(42, 68 + f * 16, 8, 4, 25.0f + windGust, cBackLimb, true, 1);
    }
    drawR(26, 95, 26, 3, 4.0f, cBlack, false);
    drawR(30, 145, 52, 20, 0.0f, cWhite, true, 3);
    drawR(10, 149, 12, 10, 0.0f, cBlack, false);
    drawR(2, 147, 10, 10, t * 30.0f + 45.0f, cGold, true, 1); // Estrella de la espuela delantera
    drawR(50, 147, 16, 14, 0.0f, cWhite, true, 2);
    drawR(32, 153, 56, 4, 0.0f, cBlack, false);

    // 5. PELVIS Y CADERA
    drawR(0, 48 + kneeFlex * 0.4f, 60, 26, 0.0f, cWhite, true, 3);

    // 6. TORSO HEROICO CON EXPANSIÓN DE PECHO AL RESPIRAR Y CHALECO
    drawR(0, torsoY - 10, 68 + chestExpand, 54, bodyAngle, cBlack, false);
    drawR(0, torsoY + 20, 52, 52, bodyAngle, cBlack, false);

    drawR(0, torsoY - 10, 62 + chestExpand, 48, bodyAngle, cWhite, false);
    drawR(0, torsoY + 20, 46, 48, bodyAngle, cWhite, false);

    // Chaleco abierto / Poncho corto sobre el pecho
    drawR(-14, torsoY - 2, 24, 58, bodyAngle, cPoncho, true, 2);
    drawR(18, torsoY - 2, 18, 56, bodyAngle, cPoncho, true, 2);

    // Pliegue central y botones de la camisa
    drawR(2, torsoY + 6, 3, 42, bodyAngle, cBlack, false);
    drawR(6, torsoY - 2, 4, 4, bodyAngle, cBlack, false);
    drawR(6, torsoY + 12, 4, 4, bodyAngle, cBlack, false);

    // INSIGNIA DE SHERIFF (Estrella dorada de 8 puntas en el pecho)
    drawR(-12, torsoY - 12, 14, 14, bodyAngle, cGold, true, 1);
    drawR(-12, torsoY - 12, 14, 14, bodyAngle + 45.0f, cGold, true, 1);
    drawR(-12, torsoY - 12, 5, 5, bodyAngle, cWhite, false);

    // 7. FUNDA (Holster) Y BALAS EN EL CINTURÓN
    drawR(24, torsoY + 60, 22, 48, bodyAngle + 6.0f, cHolster, true, 3);
    drawR(24, torsoY + 50, 24, 6, bodyAngle + 6.0f, cBlack, false); // Correa de cuero

    if (state_phase != 2 && state_phase != 4) {
        // Empuñadura asomando lista para ser tomada
        drawR(22, torsoY + 42, 14, 26, bodyAngle - 20.0f, cWhite, true, 2);
        drawR(22, torsoY + 42, 8, 16, bodyAngle - 20.0f, cGold, false); // Cachas nacaradas/doradas
        drawR(28, torsoY + 36, 6, 8, bodyAngle - 20.0f, cBlack, false);
    }

    // 8. CINTURÓN CON CARTUCHOS DORADOS Y HEBILLA
    drawR(0, torsoY + 38, 58, 16, bodyAngle, cBlack, true, 2);
    // Balas doradas alineadas en la canana
    for (int b = -2; b <= 2; ++b) {
        if (b == 0) continue; // Espacio para la hebilla
        drawR(b * 10, torsoY + 38, 4, 11, bodyAngle, cGold, false);
    }
    drawR(2, torsoY + 38, 22, 18, bodyAngle, cSilver, true, 2);
    drawR(2, torsoY + 38, 10, 8, bodyAngle, cGold, false);

    // 9. CUELLO Y PAÑUELO CON COLAS AL VIENTO
    // Colas del pañuelo flameando hacia atrás con el viento
    drawR(-24 - windGust * 0.5f, headY + 30, 32, 10, bodyAngle - 12.0f + windGust * 1.5f, cBandana, true, 2);
    drawR(-20 - windGust * 0.3f, headY + 38, 26, 8, bodyAngle - 25.0f + windGust * 1.2f, cBandana, true, 2);

    drawR(2, headY + 34, 26, 18, bodyAngle, cWhite, false);
    drawR(2, headY + 30, 42, 15, bodyAngle, cBandana, true, 2);
    drawR(14, headY + 42, 28, 28, bodyAngle + 45.0f + windGust * 0.3f, cBandana, true, 2);
    drawR(12, headY + 42, 16, 16, bodyAngle + 45.0f + windGust * 0.3f, cBlack, false);

    // 10. CABEZA, EXPRESIÓN FACIAL Y PALILLO DE TRIGO
    drawR(4, headY, 46, 46, bodyAngle + headTilt, cWhite, true, 3);
    drawR(4, headY - 11, 44, 22, bodyAngle + headTilt, cFaceShadow, false);

    // Briznas de trigo / palillo en la comisura de la boca (sube y baja sutilmente)
    if (state_phase != 4) {
        float chewAngle = (state_phase == 1) ? -5.0f : (std::sin(tLocal * 4.5f) * 8.0f + 12.0f);
        drawR(30, headY + 14, 24, 3, bodyAngle + headTilt + chewAngle, cGold, true, 1);
    }

    // Ojos y Expresiones según la fase
    if (isBlinking) {
        // Ojos cerrados (parpadeo)
        drawR(14, headY + 2, 7, 2, bodyAngle + headTilt, cBlack, false);
        drawR(-4, headY + 2, 7, 2, bodyAngle + headTilt, cBlack, false);
    } else if (state_phase == 1) {
        // Mirada de halcón en Tensión: ojos entrecerrados con brillo rojo letal que crece con tension_prog
        drawR(14, headY + 2, 8, 4, bodyAngle + headTilt - 8.0f, cWhite, false);
        drawR(-4, headY + 2, 8, 4, bodyAngle + headTilt - 5.0f, cWhite, false);
        if (tension_prog > 0.25f) {
            drawR(15, headY + 2, 5, 5, bodyAngle + headTilt, cRedGlow, false);
            drawR(-3, headY + 2, 5, 5, bodyAngle + headTilt, cRedGlow, false);
            // Destello horizontal en el ojo principal cuando la tensión está al límite
            if (tension_prog > 0.65f) {
                drawR(15, headY + 2, 18 * tension_prog, 2, 0.0f, cRedGlow, false);
            }
        }
        // Gota de sudor frío deslizándose por la mejilla en plena tensión
        float sweatY = std::fmod(tLocal * 12.0f, 18.0f);
        drawR(-14, headY - 2 + sweatY, 4, 7, bodyAngle, cSweat, true, 1);
    } else if (state_phase == 2) {
        // Ojos encendidos en el instante del disparo
        drawR(14, headY + 1, 7, 5, bodyAngle + headTilt, cFlash, false);
        drawR(-4, headY + 1, 7, 5, bodyAngle + headTilt, cFlash, false);
    } else if (state_phase == 4) {
        // Ojos de frustración "> <" por haber hecho falta
        drawR(14, headY + 4, 8, 2, bodyAngle + headTilt + 25.0f, cBlack, false);
        drawR(14, headY + 6, 8, 2, bodyAngle + headTilt - 25.0f, cBlack, false);
        // Gota de sudor gigante estilo anime/cómic en la sien
        float dropBounce = std::sin(t * 6.0f) * 2.0f;
        drawR(-18, headY - 6 + dropBounce, 8, 16, bodyAngle, cSweat, true, 2);
    } else {
        // Ojos normales con micro-movimiento de mirada
        float lookShift = (std::sin(tLocal * 0.8f) > 0.7f) ? 1.5f : 0.0f;
        drawR(14 + lookShift, headY + 2, 5, 6, bodyAngle + headTilt, cBlack, false);
        drawR(-4 + lookShift, headY + 2, 5, 6, bodyAngle + headTilt, cBlack, false);
        // Brillo blanco diminuto en los ojos para dar vida
        drawR(15 + lookShift, headY + 1, 2, 2, 0.0f, cWhite, false);
        drawR(-3 + lookShift, headY + 1, 2, 2, 0.0f, cWhite, false);
    }

    // 11. SOMBRERO CON PERSONALIDAD
    float hatTilt = bodyAngle + headTilt + ((state_phase == 1) ? 3.0f : 0.0f);
    drawR(4, headY - 24, 158, 13, hatTilt, cWhite, true, 3);
    drawR(4, headY - 18, 154, 4, hatTilt, cBlack, false);
    drawR(4, headY - 50, 56, 40, hatTilt, cWhite, true, 3);
    drawR(4, headY - 35, 56, 13, hatTilt, cHatBand, true, 2);
    drawR(4, headY - 35, 14, 11, hatTilt, cGold, true, 1); // Hebilla dorada en el sombrero
    // Pluma o muesca en el sombrero
    drawR(-22, headY - 46, 14, 6, hatTilt - 35.0f + windGust * 0.5f, cBandana, true, 1);

    // 12. BRAZO DELANTERO Y ACCIÓN DE LA MANO / REVÓLVER
    if (state_phase == 0) {
        // EN ESPERA: Balanceo natural de brazo y dedos "tamborileando" cerca del arma
        float armSwing = std::sin(tLocal * 1.6f) * 2.5f;
        float fingerTap = std::sin(tLocal * 8.0f) * 6.0f;

        drawR(36, torsoY + 4, 26, 46, bodyAngle + 10.0f + armSwing, cBlack, false);
        drawR(40, torsoY + 42, 24, 46, bodyAngle - 2.0f + armSwing, cBlack, false);

        drawR(36, torsoY + 4, 20, 42, bodyAngle + 10.0f + armSwing, cWhite, false);
        drawR(40, torsoY + 42, 18, 42, bodyAngle - 2.0f + armSwing, cWhite, false);

        drawR(40, torsoY + 62, 18, 4, bodyAngle - 2.0f + armSwing, cBackLimb, false);
        drawR(40, torsoY + 74, 16, 20, bodyAngle - 2.0f + armSwing, cWhite, true, 2);
        drawR(44, torsoY + 71, 5, 9, bodyAngle + 12.0f + fingerTap, cWhite, true, 1); // Pulgar vivo
        drawR(40, torsoY + 82, 14, 4, bodyAngle - 2.0f + fingerTap * 0.5f, cBlack, false);
        drawR(40, torsoY + 77, 10, 2, bodyAngle - 2.0f, cBlack, false);
    } else if (state_phase == 1) {
        // EN TENSIÓN: Codo afilado y dedos con espasmos nerviosos sobre la culata
        float twitch = std::sin(t * 28.0f) * (4.0f + tension_prog * 10.0f);

        drawR(38, torsoY - 8, 26, 46, bodyAngle + 24.0f, cBlack, false);
        drawR(36, torsoY + 26, 24, 44, bodyAngle - 24.0f, cBlack, false);

        drawR(38, torsoY - 8, 20, 42, bodyAngle + 24.0f, cWhite, false);
        drawR(36, torsoY + 26, 18, 40, bodyAngle - 24.0f, cWhite, false);

        drawR(46, torsoY + 9, 6, 2, bodyAngle + 24.0f, cBlack, false);
        drawR(28, torsoY + 44, 18, 4, bodyAngle - 24.0f, cBackLimb, false);

        // Mano en garra crispada con movimiento nervioso de dedos (twitch)
        drawR(24, torsoY + 46, 16, 15, bodyAngle - 15.0f, cWhite, true, 2);
        drawR(19, torsoY + 40, 6, 9, bodyAngle - 25.0f + twitch, cWhite, true, 1);       // Pulgar crispado
        drawR(27, torsoY + 53, 12, 10, bodyAngle + 15.0f - twitch * 0.8f, cWhite, true, 1); // Dedos abriéndose y cerrándose
        drawR(27, torsoY + 54, 8, 2, bodyAngle + 15.0f, cBlack, false);
    } else if (state_phase == 4) {
        // EN FALTA: Brazo caído con el revólver colgando hacia el piso y banderita de "Puf"
        float dangle = std::sin(t * 3.5f) * 5.0f;
        drawR(34, torsoY + 36, 20, 74, bodyAngle + 32.0f, cWhite, true, 3);
        drawR(42, torsoY + 74, 16, 16, bodyAngle + 32.0f, cWhite, true, 2);
        // Revólver colgando hacia abajo sin fuerza
        drawR(46, torsoY + 92, 14, 34, 75.0f + dangle, cWhite, true, 2);
        // Pequeña nubecita gris de tiro fallido en el piso
        drawR(58, torsoY + 125, 16, 12, t * 20.0f, cBackLimb, true, 1);
    } else if (state_phase == 2) {
        // DISPARO / QUICK-DRAW: Fogonazo espectacular, trazadora de bala y humo (ritmo cinematográfico con peso)
        float recoilCycle = std::fmod(t * 1.2f, 1.0f);
        float recoilKick = (recoilCycle < 0.25f) ? (1.0f - recoilCycle / 0.25f) : 0.0f;
        float recoilAngle = -recoilKick * 12.0f;
        float recoilX = -recoilKick * 5.0f;

        drawR(58 + recoilX, torsoY - 12, 78, 6, recoilAngle * 0.4f, cBlack, false);
        drawR(58 + recoilX, torsoY - 22, 78, 22, recoilAngle * 0.4f, cWhite, true, 3);
        drawR(96 + recoilX, torsoY - 22, 4, 22, recoilAngle * 0.4f, cBackLimb, false);

        float gunBaseX = 106.0f + recoilX;
        float gunBaseY = torsoY - 22.0f;
        float gunRot = recoilAngle;

        auto drawGunPart = [&](float relX, float relY, float w, float h, float partRot, SDL_Color col, bool outline = true, int bW = 2) {
            float rad = gunRot * 3.14159f / 180.0f;
            float rx = relX * std::cos(rad) - relY * std::sin(rad);
            float ry = relX * std::sin(rad) + relY * std::cos(rad);
            drawR(gunBaseX + rx, gunBaseY + ry, w, h, gunRot + partRot, col, outline, bW);
        };

        // Empuñadura con detalle dorado
        drawGunPart(6, 14, 16, 28, 20.0f, cWhite, true, 2);
        drawGunPart(6, 14, 8, 18, 20.0f, cGold, false);
        drawGunPart(8, 26, 14, 5, 20.0f, cBlack, false);
        // Martillo, Guardamonte y Armazón
        drawGunPart(2, -10, 8, 10, -15.0f - recoilKick * 25.0f, cBlack, false);
        drawGunPart(16, 8, 14, 9, 0.0f, cBlack, true, 1);
        drawGunPart(18, -3, 22, 18, 0.0f, cWhite, true, 2);
        // Tambor
        drawGunPart(32, -3, 26, 20, 0.0f, cWhite, true, 2);
        drawGunPart(32, -7, 20, 3, 0.0f, cBlack, false);
        drawGunPart(32, 1, 20, 3, 0.0f, cBlack, false);
        // Cañón pesado
        drawGunPart(62, -5, 44, 13, 0.0f, cWhite, true, 2);
        drawGunPart(52, 3, 22, 5, 0.0f, cBlack, false);
        drawGunPart(80, -13, 6, 6, 0.0f, cBlack, false);

        // Mano empuñando
        drawGunPart(0, 0, 16, 18, 0.0f, cWhite, true, 2);
        drawGunPart(2, -7, 8, 8, -20.0f, cWhite, true, 1);
        drawGunPart(9, 12, 10, 20, 15.0f, cWhite, true, 1);
        drawGunPart(9, 9, 7, 2, 15.0f, cBlack, false);
        drawGunPart(9, 15, 7, 2, 15.0f, cBlack, false);
        drawGunPart(13, 2, 9, 4, 0.0f, cWhite, false);

        // Casquillo dorado expulsado girando en el aire
        float shellProgress = recoilCycle;
        float shellX = gunBaseX + 28.0f - shellProgress * 48.0f;
        float shellY = gunBaseY - 14.0f - std::sin(shellProgress * 3.14159f) * 42.0f + shellProgress * 18.0f;
        drawR(shellX, shellY, 12, 6, shellProgress * 540.0f, cGold, true, 1);

        // Fogonazo + Línea de Trazadora Balística + Anillos de humo
        float rad = gunRot * 3.14159f / 180.0f;
        float muzzleX = gunBaseX + 94.0f * std::cos(rad) - (-5.0f) * std::sin(rad);
        float muzzleY = gunBaseY + 94.0f * std::sin(rad) + (-5.0f) * std::cos(rad);

        if (recoilKick > 0.35f) {
            // Trazadora amarilla/blanca que cruza el aire hacia el oponente
            if (recoilKick > 0.65f) {
                drawR(muzzleX + 260, muzzleY, 420, 4, gunRot, cFlash, false);
                drawR(muzzleX + 260, muzzleY, 420, 2, gunRot, cWhite, false);
            }

            // Estrella de fogonazo explosiva
            drawR(muzzleX + 18, muzzleY, 48, 48, gunRot, cFlash, true, 2);
            drawR(muzzleX + 18, muzzleY, 48, 48, gunRot + 45.0f, cFlash, true, 2);
            drawR(muzzleX + 28, muzzleY, 34, 16, gunRot, cRedGlow, false);
            drawR(muzzleX + 16, muzzleY, 22, 22, gunRot, cWhite, false);
        }

        // Humo persistente que sube desde la boca del cañón durante todo el ciclo
        float smokeDrift = recoilCycle * 55.0f;
        drawR(muzzleX + 42 + smokeDrift * 0.5f, muzzleY - 8 - smokeDrift * 0.6f, 16 + recoilCycle * 10.0f, 16 + recoilCycle * 10.0f, t * 40.0f, {225, 225, 225, 255}, true, 1);
        drawR(muzzleX + 64 + smokeDrift * 0.8f, muzzleY + 2 - smokeDrift * 0.9f, 22 + recoilCycle * 14.0f, 22 + recoilCycle * 14.0f, -t * 30.0f, {195, 195, 195, 255}, true, 1);
    }
}

struct Tumbleweed {
    float x, y;
    float speed;
    float size;
    float rotation;
    float rotationSpeed;
    bool active;
};

struct Vulture {
    float x, y;
    float radiusX, radiusY;
    float speed;
    float angle;
    float wingSpan;
};

struct DustParticle {
    float x, y;
    float speed;
    float length;
    float thickness;
    float alpha;
};

void drawDesertBackground(SDL_Renderer* ren, SDL_Texture* whiteTex, float t, Tumbleweed* weeds, int max_weeds, int state, Vulture* vultures = nullptr, int max_vultures = 0, DustParticle* dust = nullptr, int max_dust = 0, float tension_prog = 0.0f) {
    if (state == 1) { // TENSION
        SDL_SetRenderDrawColor(ren, 15, 15, 15, 255);
    } else if (state == 2) { // DRAW
        if (fmod(t, 0.4f) < 0.2f) SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        else SDL_SetRenderDrawColor(ren, 240, 240, 240, 255);
    } else {
        SDL_SetRenderDrawColor(ren, 245, 245, 245, 255);
    }
    SDL_RenderClear(ren);

    if (state != 1) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 8); // Cuadricula mas suave
        int gridOffset = static_cast<int>(t * 15.0f) % 60;
        for (int x = gridOffset - 60; x < 1920; x += 60) SDL_RenderDrawLine(ren, x, 0, x, 1080);
        for (int y = gridOffset - 60; y < 1080; y += 60) SDL_RenderDrawLine(ren, 0, y, 1920, y);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    }

    // Helper para figuras geométricas planas
    auto drawFlat = [&](float cx, float cy, float w, float h, float angle, SDL_Color color) {
        float finalX = cx - w/2.0f;
        float finalY = cy - h/2.0f;
        SDL_Point center = { (int)(w / 2.0f), (int)(h / 2.0f) };
        SDL_SetTextureColorMod(whiteTex, color.r, color.g, color.b);
        SDL_Rect cR = { (int)finalX, (int)finalY, (int)w, (int)h };
        SDL_RenderCopyEx(ren, whiteTex, nullptr, &cR, angle, &center, SDL_FLIP_NONE);
    };

    SDL_Color sunCol = (state == 1) ? SDL_Color{80, 80, 80, 255} : SDL_Color{220, 220, 220, 255};
    SDL_Color groundCol = (state == 1) ? SDL_Color{25, 25, 25, 255} : SDL_Color{230, 230, 230, 255};
    SDL_Color mountCol = (state == 1) ? SDL_Color{20, 20, 20, 255} : SDL_Color{215, 215, 215, 255};
    SDL_Color cloudCol = (state == 1) ? SDL_Color{18, 18, 18, 255} : SDL_Color{235, 235, 235, 255};
    
    // Nubes flotando lentamente
    float cloudDrift = std::fmod(t * 8.0f, 2400.0f);
    drawFlat(200 + cloudDrift * 0.5f, 150, 150, 30, 0, cloudCol);
    drawFlat(220 + cloudDrift * 0.5f, 120, 80, 30, 0, cloudCol);
    drawFlat(1600 - cloudDrift * 0.3f, 250, 200, 40, 0, cloudCol);
    drawFlat(1650 - cloudDrift * 0.3f, 210, 100, 40, 0, cloudCol);
    drawFlat(800 + cloudDrift * 0.4f, 180, 120, 30, 0, cloudCol);

    // Sol Centrado con Ondas de Calor (Mirage)
    float heatWave = std::sin(t * 5.0f) * 3.5f;
    float sunX = 960;
    float sunY = 320 + heatWave;
    float sunSize = 160 + std::sin(t * 3.0f) * 4.0f;
    drawFlat(sunX, sunY, sunSize, sunSize, 0, sunCol);
    drawFlat(sunX, sunY, sunSize, sunSize, 45, sunCol);
    for (int i = 0; i < 12; i++) {
        float a = i * 30.0f + t * 4.0f;
        float rad = a * 3.14159f / 180.0f;
        float rayLen = 40.0f + std::sin(t * 6.0f + i) * 6.0f;
        drawFlat(sunX + std::cos(rad)*110, sunY + std::sin(rad)*110, rayLen, 15, a, sunCol);
    }

    // Buitres en el cielo volando en círculos siniestros
    if (vultures && max_vultures > 0) {
        SDL_Color vultureCol = (state == 1) ? SDL_Color{50, 50, 50, 255} : SDL_Color{70, 70, 70, 255};
        for (int v = 0; v < max_vultures; ++v) {
            vultures[v].angle += vultures[v].speed;
            float vx = vultures[v].x + std::cos(vultures[v].angle) * vultures[v].radiusX;
            float vy = vultures[v].y + std::sin(vultures[v].angle) * vultures[v].radiusY;
            float flap = std::sin(t * 4.0f + v * 1.5f) * 12.0f;
            float flightAngle = (std::atan2(std::cos(vultures[v].angle)*vultures[v].radiusY, -std::sin(vultures[v].angle)*vultures[v].radiusX)) * 180.0f / 3.14159f;
            
            // Cuerpo del buitre
            drawFlat(vx, vy, 12, 5, flightAngle, vultureCol);
            // Alas en 'V'
            drawFlat(vx - 7, vy - 2, vultures[v].wingSpan, 3, flightAngle - 20.0f + flap, vultureCol);
            drawFlat(vx + 7, vy - 2, vultures[v].wingSpan, 3, flightAngle + 20.0f - flap, vultureCol);
        }
    }
    
    // Montañas (Derecha, mas lejos)
    drawFlat(1650, 700, 200, 200, 45, mountCol);
    drawFlat(1800, 730, 150, 150, 45, mountCol);
    
    // Mesa Monument Valley (Izquierda, mas chica y alejada)
    drawFlat(250, 680, 300, 40, 0, mountCol);
    drawFlat(220, 640, 200, 40, 0, mountCol);
    drawFlat(180, 520, 100, 250, 0, mountCol);
    drawFlat(140, 560, 40, 150, 0, mountCol);
    
    // Cactus brutalista (mas pequeño)
    drawFlat(1300, 620, 20, 80, 0, mountCol);
    drawFlat(1280, 600, 30, 15, 0, mountCol);
    drawFlat(1275, 580, 15, 40, 0, mountCol);

    // Suelo
    drawFlat(960, 900, 2020, 400, 0, groundCol); 

    // Línea de horizonte neo-brutalista limpia
    SDL_SetRenderDrawColor(ren, 18, 18, 18, 255);
    SDL_Rect horizLine = { 0, 698, 1920, 4 };
    SDL_RenderFillRect(ren, &horizLine); 

    // Tumbleweeds (Max 2 o 3, mas discretos)
    for(int i=0; i<max_weeds; i++) {
        if (weeds[i].active) {
            weeds[i].x += weeds[i].speed;
            weeds[i].rotation += weeds[i].rotationSpeed;
            float bounceY = weeds[i].y + std::abs(sin(weeds[i].x * 0.05f)) * -20.0f;
            
            SDL_Color weedCol = (state == 1) ? SDL_Color{35,35,35,255} : SDL_Color{190,190,190,255};
            
            drawFlat(weeds[i].x, bounceY, weeds[i].size, weeds[i].size, weeds[i].rotation, weedCol);
            drawFlat(weeds[i].x, bounceY, weeds[i].size*0.8f, weeds[i].size*0.8f, weeds[i].rotation, groundCol);
            
            drawFlat(weeds[i].x, bounceY, weeds[i].size, weeds[i].size, weeds[i].rotation + 45.0f, weedCol);
            drawFlat(weeds[i].x, bounceY, weeds[i].size*0.8f, weeds[i].size*0.8f, weeds[i].rotation + 45.0f, groundCol);
            
            if (weeds[i].x < -100 || weeds[i].x > 2000) weeds[i].active = false;
        } else {
            int active_count = 0;
            for(int j=0; j<max_weeds; j++) if(weeds[j].active) active_count++;
            
            if (active_count < 2 && (std::rand() % 1000) < 5) {
                weeds[i].active = true;
                weeds[i].x = (std::rand() % 2 == 0) ? -50 : 1970;
                weeds[i].size = 25 + std::rand() % 15; // Mas chicos
                weeds[i].y = 710 + std::rand() % 100;
                weeds[i].speed = (weeds[i].x < 0) ? (3.0f + (std::rand()%20)/10.0f) : -(3.0f + (std::rand()%20)/10.0f);
                weeds[i].rotation = 0;
                weeds[i].rotationSpeed = weeds[i].speed * 4.0f;
            }
        }
    }

    // Partículas de polvo del desierto arrastradas por el viento horizontal
    if (dust && max_dust > 0) {
        float windSpeedMult = (state == 1) ? (1.5f + tension_prog * 1.5f) : 1.0f;
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        for (int d = 0; d < max_dust; ++d) {
            dust[d].x += dust[d].speed * windSpeedMult;
            if (dust[d].x > 1950.0f) {
                dust[d].x = -30.0f - (std::rand() % 100);
                dust[d].y = 650.0f + (std::rand() % 400);
                dust[d].speed = 4.0f + (std::rand() % 6);
                dust[d].length = 15.0f + (std::rand() % 35);
            }
            Uint8 dustAlpha = (state == 1) ? 50 : 90;
            SDL_Color dCol = (state == 1) ? SDL_Color{80, 80, 80, dustAlpha} : SDL_Color{160, 155, 145, dustAlpha};
            drawFlat(dust[d].x, dust[d].y, dust[d].length * (1.0f + (windSpeedMult - 1.0f)*0.5f), dust[d].thickness, -1.5f, dCol);
        }
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    }
}

int main(int argc, char** argv) {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    // Forzamos a SDL2 a usar su driver interno (hidapi) para mandos de PS4.
    // Esto habilita sensores (giroscopio/acelerometro) y vibracion en Linux
    bool snapshotMode = false;
    std::string snapshotPath = "";
    int snapshotP1 = 0;
    int snapshotP2 = 0;
    int snapshotState = 0;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--snapshot" && i + 1 < argc) {
            snapshotMode = true;
            snapshotPath = argv[i+1];
        }
        if (std::string(argv[i]) == "--p1" && i + 1 < argc) {
            snapshotP1 = std::stoi(argv[i+1]);
        }
        if (std::string(argv[i]) == "--p2" && i + 1 < argc) {
            snapshotP2 = std::stoi(argv[i+1]);
        }
        if (std::string(argv[i]) == "--state" && i + 1 < argc) {
            snapshotState = std::stoi(argv[i+1]);
        }
    }

    // independientemente del driver generico del sistema.
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1"); // Procesa reportes de mandos en hilo dedicado evitando tirones en el render

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
    Uint32 winFlags = snapshotMode ? SDL_WINDOW_HIDDEN : SDL_WINDOW_FULLSCREEN_DESKTOP;
    int winW = snapshotMode ? 1920 : 800;
    int winH = snapshotMode ? 1080 : 600;
    SDL_Window* win = SDL_CreateWindow("Duelo de Vaqueros", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, winW, winH, winFlags);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    SDL_RenderSetLogicalSize(ren, 1920, 1080);
    
    if (!win || !ren) {
        std::fprintf(stderr, "Error creando ventana/renderer: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    TTF_Font* font = loadFont();

    SDL_Surface* wSurf = SDL_CreateRGBSurface(0, 1, 1, 32, 0,0,0,0);
    SDL_FillRect(wSurf, NULL, SDL_MapRGB(wSurf->format, 255, 255, 255));
    SDL_Texture* whiteTex = SDL_CreateTextureFromSurface(ren, wSurf);
    SDL_FreeSurface(wSurf);
    
    SDL_Texture* targetTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 1920, 1080);

    if (snapshotMode) {
        Tumbleweed testWeeds[8] = {0};
        drawDesertBackground(ren, whiteTex, 1.0f, testWeeds, 8, snapshotState);
        drawCowboy(ren, whiteTex, 480, 490, snapshotP1, false, 1.0f);
        drawCowboy(ren, whiteTex, 1440, 490, snapshotP2, true, 1.0f);
        SDL_RenderPresent(ren);

        SDL_Surface* sshot = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_ARGB8888);
        SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, sshot->pixels, sshot->pitch);
        SDL_SaveBMP(sshot, snapshotPath.c_str());
        SDL_FreeSurface(sshot);

        SDL_DestroyTexture(whiteTex);
        if (font) TTF_CloseFont(font);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        TTF_Quit();
        SDL_Quit();
        return 0;
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::fprintf(stderr, "Mix_OpenAudio fallo: %s\n", Mix_GetError());
    }
    
    const int MAX_WEEDS = 8;
    Tumbleweed tumbleweeds[MAX_WEEDS] = {0};

    const int MAX_VULTURES = 3;
    Vulture vultures[MAX_VULTURES] = {
        { 960.0f, 220.0f, 320.0f, 65.0f, 0.008f, 0.0f, 18.0f },
        { 1120.0f, 260.0f, 240.0f, 50.0f, -0.011f, 1.8f, 14.0f },
        { 820.0f, 190.0f, 380.0f, 80.0f, 0.006f, 3.5f, 22.0f }
    };

    const int MAX_DUST = 24;
    DustParticle dustParticles[MAX_DUST];
    for (int d = 0; d < MAX_DUST; ++d) {
        dustParticles[d].x = (float)(std::rand() % 1920);
        dustParticles[d].y = 650.0f + (float)(std::rand() % 400);
        dustParticles[d].speed = 4.0f + (float)(std::rand() % 7);
        dustParticles[d].length = 15.0f + (float)(std::rand() % 35);
        dustParticles[d].thickness = 2.0f + (float)(std::rand() % 2);
        dustParticles[d].alpha = 70.0f;
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

    // SFX Sintetizados Procedurales de alta calidad
    Mix_Chunk* sfxHeartbeat = generateToneChunk(120, 55, 12.0f, 0.9f, 0);   // Latido grave profundo
    Mix_Chunk* sfxTick      = generateToneChunk(45, 900, 30.0f, 0.5f, 1);   // Tic de reloj
    Mix_Chunk* sfxBell      = generateToneChunk(900, 320, 3.5f, 0.6f, 2);   // Campana de iglesia lejana
    Mix_Chunk* sfxHammer    = generateToneChunk(60, 1400, 25.0f, 0.8f, 1);  // Martillo / Resistencia mecánica
    Mix_Chunk* sfxTimeWarp  = generateToneChunk(1850, 180, 1.2f, 0.95f, 4);  // Sub-drop cinematográfico de cámara lenta
    Mix_Chunk* sfxBulletFly = generateToneChunk(1700, 1400, 1.0f, 0.75f, 5); // Silbido supersónico / zumbido Doppler
    Mix_Chunk* sfxImpact    = generateToneChunk(450, 75, 7.5f, 1.0f, 6);     // Impacto demoledor y sombrerazo seco

    auto playFireSound = [&]() {
        if (sfxDisparoPC) {
            Mix_PlayChannel(-1, sfxDisparoPC, 0);
        }
        if (controller_audio > 0 && wav_buffer != nullptr) {
            SDL_ClearQueuedAudio(controller_audio);
            SDL_QueueAudio(controller_audio, wav_buffer, wav_length);
            SDL_PauseAudioDevice(controller_audio, 0);
        }
    };

    // Helper para establecer colores en la barra LED del DualShock 4
    auto setControllerLED = [](SDL_GameController* gc, Uint8 r, Uint8 g, Uint8 b) {
        if (!gc) return;
        SDL_GameControllerSetLED(gc, r, g, b);
    };

    Player p1, p2;
    GameState state = STATE_WAITING;
    Uint32 tension_start_time = 0;
    Uint32 tension_duration = 0;
    Uint32 draw_start_time = 0;
    Uint32 reaction_time_ms = 0;

    // Estado del impacto cinematográfico (Flashbang + Hit-Stop + Bullet-Time)
    Uint32 hit_time = 0;
    int bullet_winner = 0;
    bool impact_played = false;
    
    int winner = 0; // 0: nadie, 1: J1, 2: J2, 3: empate

    // Control de latidos y efectos periódicos en tensión
    Uint32 last_heartbeat_time = 0;
    Uint32 last_tick_time = 0;

    // Inicialización y asignación eficiente de mandos
    auto setupController = [&](Player& p, SDL_GameController* gc) {
        p.gc = gc;
        p.ready = false;
        p.foul = false;
        p.has_drawn = false;
        p.has_gyro = SDL_GameControllerHasSensor(gc, SDL_SENSOR_GYRO);
        p.has_accel = SDL_GameControllerHasSensor(gc, SDL_SENSOR_ACCEL);
        if (p.has_gyro) SDL_GameControllerSetSensorEnabled(gc, SDL_SENSOR_GYRO, SDL_TRUE);
        if (p.has_accel) SDL_GameControllerSetSensorEnabled(gc, SDL_SENSOR_ACCEL, SDL_TRUE);
        setControllerLED(gc, 80, 180, 255); // Azul tenue inicial
    };

    auto assignController = [&](int device_index) {
        if (!SDL_IsGameController(device_index)) return;
        SDL_GameController* gc = SDL_GameControllerOpen(device_index);
        if (!gc) return;

        if (p1.gc == gc || p2.gc == gc) return;

        if (!p1.gc) {
            setupController(p1, gc);
        } else if (!p2.gc) {
            setupController(p2, gc);
        } else {
            SDL_GameControllerClose(gc);
        }
    };

    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        assignController(i);
    }

    bool running = true;
    while (running) {
        Uint32 frameStart = SDL_GetTicks();

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = false;
            } else if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (ev.key.keysym.sym == SDLK_SPACE || ev.key.keysym.sym == SDLK_RETURN) {
                    if (state == STATE_WAITING) {
                        p1.ready = true;
                        if (p2.gc) p2.ready = true;
                        if (sfxHammer) Mix_PlayChannel(-1, sfxHammer, 0);
                    } else if (state == STATE_RESULT) {
                        state = STATE_WAITING;
                        p1.ready = p2.ready = p1.foul = p2.foul = false;
                        winner = 0;
                        reaction_time_ms = 0;
                        hit_time = 0;
                        bullet_winner = 0;
                        impact_played = false;
                        setControllerLED(p1.gc, 80, 180, 255);
                        setControllerLED(p2.gc, 80, 180, 255);
                    }
                }
            } else if (ev.type == SDL_CONTROLLERDEVICEADDED) {
                assignController(ev.cdevice.which);
            } else if (ev.type == SDL_CONTROLLERDEVICEREMOVED) {
                SDL_JoystickID jid = ev.cdevice.which;
                if (p1.gc && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(p1.gc)) == jid) {
                    SDL_GameControllerClose(p1.gc);
                    p1.gc = nullptr;
                    p1.ready = false;
                } else if (p2.gc && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(p2.gc)) == jid) {
                    SDL_GameControllerClose(p2.gc);
                    p2.gc = nullptr;
                    p2.ready = false;
                }
            } else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
                if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
                    running = false;
                }
                
                if (state == STATE_WAITING && ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                    SDL_JoystickID id = ev.cbutton.which;
                    SDL_JoystickID id1 = p1.gc ? SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(p1.gc)) : -1;
                    SDL_JoystickID id2 = p2.gc ? SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(p2.gc)) : -1;
                    
                    if (id == id1 && isMatchingPose(p1.accel, p1.cal_down, 0.5f)) {
                        p1.ready = true;
                        setControllerLED(p1.gc, 0, 255, 80); // Verde listo
                        if (sfxHammer) Mix_PlayChannel(-1, sfxHammer, 0);
                    }
                    if (id == id2 && isMatchingPose(p2.accel, p2.cal_down, 0.5f)) {
                        p2.ready = true;
                        setControllerLED(p2.gc, 0, 255, 80); // Verde listo
                        if (sfxHammer) Mix_PlayChannel(-1, sfxHammer, 0);
                    }
                }
                
                if (state == STATE_RESULT && ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                    state = STATE_WAITING;
                    p1.ready = p2.ready = p1.foul = p2.foul = false;
                    winner = 0;
                    reaction_time_ms = 0;
                    hit_time = 0;
                    bullet_winner = 0;
                    impact_played = false;
                    setControllerLED(p1.gc, 80, 180, 255);
                    setControllerLED(p2.gc, 80, 180, 255);
                }
            }
        }

        auto readSensors = [](Player& p) {
            if (!p.gc) return;
            if (p.has_gyro) {
                SDL_GameControllerGetSensorData(p.gc, SDL_SENSOR_GYRO, p.gyro, 3);
            }
            if (p.has_accel) {
                SDL_GameControllerGetSensorData(p.gc, SDL_SENSOR_ACCEL, p.accel, 3);
            }
        };

        auto checkDrawn = [](Player& p) {
            if (!p.gc) return;
            if (std::abs(p.gyro[0]) > 4.0f || std::abs(p.gyro[1]) > 4.0f || std::abs(p.gyro[2]) > 4.0f) {
                p.has_drawn = true;
            }
            if (isMatchingPose(p.accel, p.cal_forward, 0.8f)) {
                p.has_drawn = true;
            }
        };

        auto hasPulledTrigger = [](Player& p) -> bool {
            if (!p.gc) return false;
            return SDL_GameControllerGetAxis(p.gc, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 10000;
        };

        readSensors(p1);
        readSensors(p2);

        // Feedback de resistencia mecánica previa al disparar (sensación de amartillado)
        auto handleTriggerResistance = [&](Player& p) {
            if (!p.gc) return;
            Sint16 triggerVal = SDL_GameControllerGetAxis(p.gc, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
            if (triggerVal > 4000 && triggerVal < 9000) {
                // Micro-resistencia / click antes del punto de quiebre
                SDL_GameControllerRumble(p.gc, 0x1800, 0x0000, 25);
            }
        };
        handleTriggerResistance(p1);
        handleTriggerResistance(p2);

        if (state == STATE_WAITING) {
            p1.has_drawn = false;
            p2.has_drawn = false;
            
            int mandos_conectados = (p1.gc ? 1 : 0) + (p2.gc ? 1 : 0);
            bool p1_listo = p1.ready;
            bool p2_listo = p2.ready;

            bool listos_para_empezar = (p1_listo && p2_listo);
            if (mandos_conectados <= 1 && p1_listo) {
                listos_para_empezar = true; 
            }

            if (listos_para_empezar) {
                state = STATE_TENSION;
                tension_start_time = SDL_GetTicks();
                tension_duration = 2400 + (std::rand() % 4200);
                last_heartbeat_time = tension_start_time;
                last_tick_time = tension_start_time;
                if (bgmTension) Mix_PlayChannel(1, bgmTension, -1);
                if (sfxBell) Mix_PlayChannel(2, sfxBell, 0); // Campana de iglesia marcando el inicio
            }
        }
        else if (state == STATE_TENSION) {
            checkDrawn(p1);
            checkDrawn(p2);

            Uint32 elapsedTension = SDL_GetTicks() - tension_start_time;

            // LED de los mandos: Rojo tenue misterioso y FIJO (no cambia con el tiempo para NO delatar el duelo)
            setControllerLED(p1.gc, 180, 20, 20);
            setControllerLED(p2.gc, 180, 20, 20);

            // Latido cardíaco en el mando: ritmo constante y misterioso cada 850ms (no se acelera para no delatar)
            if (SDL_GetTicks() - last_heartbeat_time > 850) {
                last_heartbeat_time = SDL_GetTicks();
                // Doble pulso cardíaco ('tum-tum')
                Uint16 rumblePower = 0x4500;
                if (p1.gc) SDL_GameControllerRumble(p1.gc, rumblePower, rumblePower / 2, 60);
                if (p2.gc) SDL_GameControllerRumble(p2.gc, rumblePower, rumblePower / 2, 60);
                if (sfxHeartbeat) Mix_PlayChannel(-1, sfxHeartbeat, 0);
            }

            // Tic-tac de reloj mecánico cada 500ms
            if (SDL_GetTicks() - last_tick_time > 500) {
                last_tick_time = SDL_GetTicks();
                if (sfxTick) Mix_PlayChannel(-1, sfxTick, 0);
            }
            
            // En tensión, moverse (desenfundar) o disparar es falta
            bool p1_foul = p1.has_drawn || hasPulledTrigger(p1);
            bool p2_foul = p2.has_drawn || hasPulledTrigger(p2);
            
            if (p1_foul || p2_foul) {
                playFireSound();
                Mix_HaltChannel(1);
                state = STATE_RESULT;
                if (p1_foul) { 
                    p1.foul = true; winner = 2; 
                    setControllerLED(p1.gc, 255, 0, 0);
                    setControllerLED(p2.gc, 0, 255, 100);
                } else { 
                    p2.foul = true; winner = 1; 
                    setControllerLED(p2.gc, 255, 0, 0);
                    setControllerLED(p1.gc, 0, 255, 100);
                }
            } else if (elapsedTension > tension_duration) {
                state = STATE_DRAW;
                draw_start_time = SDL_GetTicks();
                // Destello Blanco Cegador en los mandos al momento del "DRAW"
                setControllerLED(p1.gc, 255, 255, 255);
                setControllerLED(p2.gc, 255, 255, 255);
                // Vibración de aviso de desenfunde
                if (p1.gc) SDL_GameControllerRumble(p1.gc, 0xFFFF, 0xFFFF, 180);
                if (p2.gc) SDL_GameControllerRumble(p2.gc, 0xFFFF, 0xFFFF, 180);
                Mix_HaltChannel(1);
            }
        }
        else if (state == STATE_DRAW) {
            checkDrawn(p1);
            checkDrawn(p2);
            
            bool p1_fire = p1.has_drawn && hasPulledTrigger(p1);
            bool p2_fire = p2.has_drawn && hasPulledTrigger(p2);
            
            if (p1_fire || p2_fire) {
                playFireSound();
                hit_time = SDL_GetTicks();
                reaction_time_ms = hit_time - draw_start_time;
                impact_played = false;

                // Clímax cinematográfico: sub-drop de cámara lenta y silbido balístico
                if (sfxTimeWarp) Mix_PlayChannel(3, sfxTimeWarp, 0);
                if (sfxBulletFly) Mix_PlayChannel(4, sfxBulletFly, 0);

                // Retroceso seco / Kick violento en el gatillo (120ms a tope)
                if (p1_fire && p1.gc) SDL_GameControllerRumble(p1.gc, 0xFFFF, 0xFFFF, 120);
                if (p2_fire && p2.gc) SDL_GameControllerRumble(p2.gc, 0xFFFF, 0xFFFF, 120);

                if (p1_fire && !p2_fire) {
                    winner = 1; bullet_winner = 1;
                    setControllerLED(p1.gc, 0, 255, 100);
                    setControllerLED(p2.gc, 255, 20, 20);
                } else if (p2_fire && !p1_fire) {
                    winner = 2; bullet_winner = 2;
                    setControllerLED(p2.gc, 0, 255, 100);
                    setControllerLED(p1.gc, 255, 20, 20);
                } else {
                    winner = 3; bullet_winner = 3;
                    setControllerLED(p1.gc, 255, 200, 0);
                    setControllerLED(p2.gc, 255, 200, 0);
                }
                state = STATE_SHOWDOWN; // Entra a la secuencia cinematográfica épica
            }
        }
        else if (state == STATE_SHOWDOWN) {
            Uint32 elapsedShowdown = SDL_GetTicks() - hit_time;
            
            // Vibración háptica sub-grave continua en los mandos durante el vuelo de la bala
            if (elapsedShowdown >= 100 && elapsedShowdown < 1950) {
                if (p1.gc) SDL_GameControllerRumble(p1.gc, 0x1A00, 0x0000, 50);
                if (p2.gc) SDL_GameControllerRumble(p2.gc, 0x1A00, 0x0000, 50);
            }

            // Impacto contundente y violento a los 1950ms
            if (elapsedShowdown >= 1950 && !impact_played) {
                impact_played = true;
                if (sfxImpact) Mix_PlayChannel(5, sfxImpact, 0);
                if (p1.gc) SDL_GameControllerRumble(p1.gc, 0xFFFF, 0xFFFF, 220);
                if (p2.gc) SDL_GameControllerRumble(p2.gc, 0xFFFF, 0xFFFF, 220);
            }

            if (elapsedShowdown >= 2500) { // Secuencia total extendida a 2.5s
                state = STATE_RESULT;
            }
        }

        // --- RENDERIZADO NEO-BRUTALISTA ---
        float currTime = SDL_GetTicks() * 0.001f;
        
        float tension_prog = 0.0f;
        if (state == STATE_TENSION || state == STATE_DRAW) {
            float elapsed = (float)(state == STATE_TENSION ? (SDL_GetTicks() - tension_start_time) : tension_duration);
            tension_prog = (-cos(elapsed * 0.0025f) + 1.0f) * 0.5f; 
            tension_prog = tension_prog * tension_prog;
        } else if (state == STATE_SHOWDOWN) {
            tension_prog = 1.0f;
        }

        if (targetTex) {
            SDL_SetRenderTarget(ren, targetTex);
        }

        drawDesertBackground(ren, whiteTex, currTime, tumbleweeds, 8, state == STATE_SHOWDOWN ? 2 : state, vultures, 3, dustParticles, 24, tension_prog);

        // Determinar fases de animación de los vaqueros
        int phaseP1 = 0;
        int phaseP2 = 0;
        
        if (state == STATE_WAITING) {
            phaseP1 = 0; phaseP2 = 0;
        } else if (state == STATE_TENSION) {
            phaseP1 = 1; phaseP2 = 1;
        } else if (state == STATE_DRAW) {
            phaseP1 = p1.has_drawn ? 2 : 1; 
            phaseP2 = p2.has_drawn ? 2 : 1;
        } else if (state == STATE_SHOWDOWN) {
            Uint32 elapsedShowdown = SDL_GetTicks() - hit_time;
            if (elapsedShowdown < 1950) {
                // Durante el vuelo balístico en cámara lenta: ganador firme disparando, rival paralizado
                phaseP1 = (winner == 1) ? 2 : 1;
                phaseP2 = (winner == 2) ? 2 : 1;
                if (winner == 3) { phaseP1 = 2; phaseP2 = 2; }
            } else {
                // Al impactar: caída dramática en cámara lenta
                phaseP1 = (winner == 1) ? 2 : 3;
                phaseP2 = (winner == 2) ? 2 : 3;
                if (winner == 3) { phaseP1 = 3; phaseP2 = 3; }
            }
        } else if (state == STATE_RESULT) {
            if (p1.foul) { phaseP1 = 4; phaseP2 = 0; }
            else if (p2.foul) { phaseP1 = 0; phaseP2 = 4; }
            else if (winner == 1) { phaseP1 = 2; phaseP2 = 3; }
            else if (winner == 2) { phaseP1 = 3; phaseP2 = 2; }
            else if (winner == 3) { phaseP1 = 3; phaseP2 = 3; }
        }

        // Determinar si se dibuja el sombrero en el suelo (se oculta mientras vuela en el aire durante el impacto)
        Uint32 elShowdownNow = (state == STATE_SHOWDOWN) ? (SDL_GetTicks() - hit_time) : 0;
        bool showP1GroundHat = !(winner == 2 && state == STATE_SHOWDOWN && elShowdownNow < 2400);
        bool showP2GroundHat = !(winner == 1 && state == STATE_SHOWDOWN && elShowdownNow < 2400);

        // Dibujar Vaqueros Brutalistas
        drawCowboy(ren, whiteTex, 480, 490, phaseP1, false, currTime, tension_prog, showP1GroundHat);
        drawCowboy(ren, whiteTex, 1440, 490, phaseP2, true, currTime, tension_prog, showP2GroundHat);

        // =========================================================================
        // SECUENCIA CINEMÁTICA: BULLET-TIME ÉPICO Y ESPECTACULAR
        // =========================================================================
        if (state == STATE_SHOWDOWN && bullet_winner > 0) {
            Uint32 elapsedShowdown = SDL_GetTicks() - hit_time;
            int dir = (bullet_winner == 1) ? 1 : -1;
            // Boca del cañón calculada exactamente según la geometría del revólver en pantalla
            float startX = (bullet_winner == 1) ? 755.0f : 1165.0f;
            float targetX = (bullet_winner == 1) ? 1390.0f : 530.0f;
            float bulletY = 442.0f;
            float cylinderX = (bullet_winner == 1) ? 660.0f : 1260.0f;

            // FASE 2: VUELO BALÍSTICO EN ULTRA CÁMARA LENTA (100ms a 1950ms)
            if (elapsedShowdown >= 100 && elapsedShowdown < 1950) {
                float rawProg = (float)(elapsedShowdown - 100) / 1850.0f; // 0.0 a 1.0
                // Curva cinemática: desacelera a ultra slow-mo y avanza implacable
                float bulletProg = rawProg * rawProg * (3.0f - 2.0f * rawProg);
                float currentBulletX = startX + (targetX - startX) * bulletProg;

                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

                // 1. VIÑETA OSCURA CINEMATOGRÁFICA EN ESCENA (Enfoca el ojo en la trayectoria)
                SDL_SetRenderDrawColor(ren, 10, 10, 14, 55);
                SDL_Rect topVignette = { 0, 0, 1920, 200 };
                SDL_Rect botVignette = { 0, 880, 1920, 200 };
                SDL_RenderFillRect(ren, &topVignette);
                SDL_RenderFillRect(ren, &botVignette);

                // 2. LÍNEAS DE VELOCIDAD HORIZONTALES HIPERSÓNICAS (Speed Lines dinámicas estilo anime)
                for (int s = 0; s < 16; ++s) {
                    int lineY = 240 + s * 38;
                    int lineLen = 140 + ((s * 47) % 260);
                    float speedJitter = std::sin(currTime * 35.0f + s * 1.5f) * 40.0f;
                    int lineX = (int)currentBulletX - dir * (120 + ((s * 61) % 480) + (int)speedJitter);
                    Uint8 lineAlpha = (Uint8)(35 + (s % 4) * 22 + std::sin(currTime * 25.0f + s) * 15);
                    SDL_SetRenderDrawColor(ren, 25, 25, 30, lineAlpha);
                    SDL_Rect sLine = { lineX, lineY, lineLen, (s % 3 == 0) ? 3 : 2 };
                    SDL_RenderFillRect(ren, &sLine);
                }

                // 3. HUMO DE PÓLVORA VOLUMÉTRICO EXPANDIÉNDOSE LENTAMENTE EN LA BOCA DEL CAÑÓN
                float smokeProgress = std::min(1.0f, (float)(elapsedShowdown - 100) / 900.0f);
                for (int sm = 0; sm < 6; ++sm) {
                    float sOffset = (float)sm * 14.0f * smokeProgress;
                    float sRadius = 16.0f + (float)sm * 10.0f + smokeProgress * 28.0f;
                    float smX = startX + dir * sOffset - (float)sm * 4.0f * dir;
                    float smY = bulletY - 12.0f - smokeProgress * 22.0f - (float)sm * 5.0f;
                    Uint8 smAlpha = (Uint8)(std::max(0.0f, (1.0f - smokeProgress) * (140 - sm * 18)));
                    SDL_SetRenderDrawColor(ren, 210, 205, 195, smAlpha);
                    SDL_Rect smR = { (int)(smX - sRadius / 2), (int)(smY - sRadius / 2), (int)sRadius, (int)sRadius };
                    SDL_RenderFillRect(ren, &smR);
                }

                // 4. CHISPAS INCANDESCENTES FLOTANDO CERCA DE LA BOCA DEL CAÑÓN
                for (int sp = 0; sp < 6; ++sp) {
                    float sparkT = std::fmod((float)(elapsedShowdown - 100) * 0.002f + sp * 0.18f, 1.0f);
                    float spX = startX + dir * (sparkT * 90.0f + sp * 8.0f);
                    float spY = bulletY - 15.0f - std::sin(sparkT * 3.14159f) * 35.0f + sp * 6.0f;
                    Uint8 spAlpha = (Uint8)(255 * (1.0f - sparkT));
                    SDL_SetRenderDrawColor(ren, 255, 190, 40, spAlpha);
                    SDL_Rect spR = { (int)spX, (int)spY, 4, 4 };
                    SDL_RenderFillRect(ren, &spR);
                }

                // 5. CASQUILLO DORADO EN CÁMARA LENTA VOLANDO DESDE EL TAMBOR
                float shellT = std::min(1.0f, rawProg * 1.55f);
                float shellX = cylinderX - dir * (shellT * 85.0f);
                float shellY = bulletY - 14.0f - std::sin(shellT * 3.14159f) * 65.0f + shellT * 48.0f;

                // Cuerpo de latón brillante
                SDL_SetRenderDrawColor(ren, 245, 215, 50, 255);
                SDL_Rect shellR = { (int)shellX - 8, (int)shellY - 4, 16, 8 };
                SDL_RenderFillRect(ren, &shellR);
                // Borde de la vaina y culote
                SDL_SetRenderDrawColor(ren, 20, 20, 20, 255);
                SDL_RenderDrawRect(ren, &shellR);
                SDL_SetRenderDrawColor(ren, 255, 245, 150, 255);
                SDL_Rect shellRim = { dir == 1 ? (int)shellX - 8 : (int)shellX + 6, (int)shellY - 5, 3, 10 };
                SDL_RenderFillRect(ren, &shellRim);

                // 6. ONDAS DE CHOQUE CÓNICAS EXPANSIVAS (Mach Shockwave Cones & Shock Diamonds)
                for (int c = 1; c <= 5; ++c) {
                    float shockOffset = (float)c * 95.0f;
                    float shockX = currentBulletX - dir * shockOffset;
                    if ((dir == 1 && shockX > startX) || (dir == -1 && shockX < startX)) {
                        float coneProgress = 1.0f - (float)c * 0.18f;
                        float shockH = 34.0f + (float)c * 24.0f;
                        float shockW = 14.0f + (float)c * 10.0f;
                        
                        // Diamantes de choque
                        SDL_SetRenderDrawColor(ren, 255, 240, 100, (Uint8)(90 * coneProgress));
                        SDL_Rect diamond = { (int)(shockX - 6), (int)(bulletY - 6), 12, 12 };
                        SDL_RenderDrawRect(ren, &diamond);

                        // Anillo de refracción acústica
                        SDL_SetRenderDrawColor(ren, 255, 255, 255, (Uint8)(175 * coneProgress));
                        SDL_Rect shockR = { (int)(shockX - shockW / 2), (int)(bulletY - shockH / 2), (int)shockW, (int)shockH };
                        SDL_RenderDrawRect(ren, &shockR);
                        
                        // Vectores envolventes del cono Mach
                        SDL_SetRenderDrawColor(ren, 255, 225, 60, (Uint8)(130 * coneProgress));
                        SDL_RenderDrawLine(ren, (int)currentBulletX, (int)bulletY, (int)shockX, (int)(bulletY - shockH / 2));
                        SDL_RenderDrawLine(ren, (int)currentBulletX, (int)bulletY, (int)shockX, (int)(bulletY + shockH / 2));
                    }
                }

                // 7. ESTELA DE VACÍO HIPERSÓNICA SUPERSÓNICA (Tracer multicapa que nace directamente en la boca de la pistola)
                float trailStart = (dir == 1) ? startX : currentBulletX;
                float trailLen = (dir == 1) ? std::max(0.0f, currentBulletX - startX) : std::max(0.0f, startX - currentBulletX);
                
                if (trailLen > 1.0f) {
                    // Resplandor ardiente exterior de plasma
                    SDL_SetRenderDrawColor(ren, 255, 150, 30, 95);
                    SDL_Rect trOuter = { (int)trailStart, (int)(bulletY - 6), (int)trailLen, 12 };
                    SDL_RenderFillRect(ren, &trOuter);

                    // Resplandor dorado supersónico
                    SDL_SetRenderDrawColor(ren, 255, 215, 50, 160);
                    SDL_Rect trGlow = { (int)trailStart, (int)(bulletY - 4), (int)trailLen, 8 };
                    SDL_RenderFillRect(ren, &trGlow);
                    
                    // Núcleo láser brillante
                    SDL_SetRenderDrawColor(ren, 255, 245, 130, 240);
                    SDL_Rect trCore = { (int)trailStart, (int)(bulletY - 2), (int)trailLen, 4 };
                    SDL_RenderFillRect(ren, &trCore);
                    
                    // Filamento blanco puro de calor extremo
                    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                    SDL_Rect trWhite = { (int)trailStart, (int)(bulletY - 1), (int)trailLen, 2 };
                    SDL_RenderFillRect(ren, &trWhite);
                }

                // 8. CHISPAS DESPRENDIDAS DE LA TRAZADORA EN EL AIRE (Solo en el trayecto desde el cañón)
                for (int trSp = 0; trSp < 8; ++trSp) {
                    float spPos = (float)trSp * 55.0f;
                    float sX = currentBulletX - dir * spPos;
                    if ((dir == 1 && sX >= startX) || (dir == -1 && sX <= startX)) {
                        float sY = bulletY + std::sin(currTime * 15.0f + trSp) * 6.0f;
                        SDL_SetRenderDrawColor(ren, 255, 235, 120, (Uint8)(220 - trSp * 24));
                        SDL_Rect sDot = { (int)sX, (int)sY, 3, 3 };
                        SDL_RenderFillRect(ren, &sDot);
                    }
                }

                // 9. PROYECTIL .45 CALIBER DE PRECISIÓN MILIMÉTRICA CON ROTACIÓN DE ESTRIADO
                int bX = (int)currentBulletX;
                int bY = (int)bulletY;
                
                // Cuerpo de cobre macizo
                SDL_SetRenderDrawColor(ren, 235, 175, 30, 255);
                SDL_Rect bBody = { dir == 1 ? bX - 22 : bX, bY - 7, 24, 14 };
                SDL_RenderFillRect(ren, &bBody);

                // Bisel inferior de sombra del proyectil
                SDL_SetRenderDrawColor(ren, 175, 115, 15, 255);
                SDL_Rect bBevel = { dir == 1 ? bX - 22 : bX, bY + 3, 24, 4 };
                SDL_RenderFillRect(ren, &bBevel);

                // Ranuras de estriado giratorio (Rifling spin visible en slow motion)
                float riflingPhase = std::fmod(rawProg * 140.0f, 18.0f);
                SDL_SetRenderDrawColor(ren, 20, 20, 20, 255);
                SDL_RenderDrawRect(ren, &bBody);
                for (int r = 0; r < 3; ++r) {
                    int rLineX = (dir == 1) ? (bX - 20 + (int)(r * 7 + riflingPhase) % 20) : (bX + 2 + (int)(r * 7 + riflingPhase) % 20);
                    SDL_RenderDrawLine(ren, rLineX, bY - 6, rLineX, bY + 6);
                }
                
                // Ojiva puntiaguda dorada
                SDL_SetRenderDrawColor(ren, 255, 235, 95, 255);
                SDL_Rect bTip = { dir == 1 ? bX + 2 : bX - 14, bY - 5, 14, 10 };
                SDL_RenderFillRect(ren, &bTip);
                SDL_SetRenderDrawColor(ren, 20, 20, 20, 255);
                SDL_RenderDrawRect(ren, &bTip);
                
                // Destello especular metálico brillante en la ojiva
                SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                SDL_Rect bSpec = { dir == 1 ? bX + 6 : bX - 10, bY - 4, 8, 3 };
                SDL_RenderFillRect(ren, &bSpec);

                // 10. REACCIÓN DEL RIVAL ANTE LA INMINENCIA DEL DISPARO (bulletProg > 0.70f)
                if (bulletProg > 0.70f) {
                    float panicProg = (bulletProg - 0.70f) / 0.30f;
                    float rivalX = (bullet_winner == 1) ? 1440.0f : 480.0f;
                    
                    // Ojos del rival abriéndose en shock
                    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                    SDL_Rect panicEye = { (int)(rivalX - dir * 18), 376, 8, 6 };
                    SDL_RenderFillRect(ren, &panicEye);
                    SDL_SetRenderDrawColor(ren, 15, 15, 15, 255);
                    SDL_RenderDrawRect(ren, &panicEye);

                    // Gotas de sudor congeladas saliendo despedidas de la sien
                    for (int sw = 0; sw < 2; ++sw) {
                        float swX = rivalX - dir * (28.0f + sw * 14.0f + panicProg * 18.0f);
                        float swY = 370.0f - sw * 12.0f + panicProg * 10.0f;
                        SDL_SetRenderDrawColor(ren, 140, 220, 255, (Uint8)(230 * panicProg));
                        SDL_Rect swR = { (int)swX, (int)swY, 5, 7 };
                        SDL_RenderFillRect(ren, &swR);
                    }

                    // Ondas sónicas que deforman y levantan el ala del sombrero
                    float brimLift = panicProg * 24.0f;
                    SDL_SetRenderDrawColor(ren, 255, 235, 70, (Uint8)(220 * panicProg));
                    SDL_Rect shockFront = { (int)(rivalX - dir * 28), (int)(365 - brimLift * 0.4f), 10, 50 };
                    SDL_RenderFillRect(ren, &shockFront);
                }

                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
            }
            
            // FASE 3: EL IMPACTO CINEMATOGRÁFICO Y SOMBRERO VOLANDO EN 720° (1950ms a 2500ms)
            if (elapsedShowdown >= 1950) {
                float hitProg = (float)(elapsedShowdown - 1950) / 550.0f; // 0.0 a 1.0
                float rivalX = (bullet_winner == 1) ? 1440.0f : 480.0f;

                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

                // 1. DESTELLO DE IMPACTO Y CHISPAS RADIALES (Primeros 150ms del impacto)
                if (elapsedShowdown < 2100) {
                    float impactFlashProg = (float)(elapsedShowdown - 1950) / 150.0f;
                    SDL_SetRenderDrawColor(ren, 255, 255, 255, (Uint8)(255 * (1.0f - impactFlashProg)));
                    SDL_Rect hitStar = { (int)(rivalX - 25), 440, 50, 18 };
                    SDL_RenderFillRect(ren, &hitStar);
                    
                    // Chispas que salen despedidas en abanico
                    for (int k = 0; k < 8; ++k) {
                        float spkAngle = (float)k * 0.785f;
                        float spkDist = impactFlashProg * 60.0f;
                        int spkX = (int)(rivalX + std::cos(spkAngle) * spkDist);
                        int spkY = (int)(450 + std::sin(spkAngle) * spkDist);
                        SDL_SetRenderDrawColor(ren, 255, 220, 60, (Uint8)(255 * (1.0f - impactFlashProg)));
                        SDL_Rect spkDot = { spkX, spkY, 5, 5 };
                        SDL_RenderFillRect(ren, &spkDot);
                    }
                }

                // 2. TRAYECTORIA PARABÓLICA Y GIRO ACROBÁTICO EN 720° DEL SOMBRERO
                float hatFlyX = rivalX + dir * (hitProg * 220.0f);
                float hatFlyY = 365.0f - std::sin(hitProg * 3.14159f) * 190.0f + hitProg * 105.0f;
                float hatAngle = dir * (hitProg * 720.0f); // Dos giros completos espectaculares

                // Ala del sombrero
                SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                SDL_Rect flyBrim = { (int)(hatFlyX - 54), (int)(hatFlyY - 8), 108, 16 };
                SDL_RenderFillRect(ren, &flyBrim);
                SDL_SetRenderDrawColor(ren, 18, 18, 18, 255);
                SDL_RenderDrawRect(ren, &flyBrim);

                // Copa del sombrero con banda oscura y hebilla dorada
                SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                SDL_Rect flyCrown = { (int)(hatFlyX - 24), (int)(hatFlyY - 30), 48, 24 };
                SDL_RenderFillRect(ren, &flyCrown);
                SDL_SetRenderDrawColor(ren, 35, 35, 40, 255);
                SDL_Rect flyBand = { (int)(hatFlyX - 24), (int)(hatFlyY - 14), 48, 8 };
                SDL_RenderFillRect(ren, &flyBand);
                SDL_SetRenderDrawColor(ren, 245, 205, 45, 255); // Hebilla dorada
                SDL_Rect flyBuckle = { (int)(hatFlyX - 6), (int)(hatFlyY - 14), 12, 8 };
                SDL_RenderFillRect(ren, &flyBuckle);
                SDL_SetRenderDrawColor(ren, 18, 18, 18, 255);
                SDL_RenderDrawRect(ren, &flyCrown);

                // 3. FRAGMENTOS DE FIELTRO DESPRENDIDOS EN EL AIRE
                for (int fr = 0; fr < 4; ++fr) {
                    float frX = rivalX + dir * (hitProg * (120.0f + fr * 35.0f));
                    float frY = 380.0f - std::sin(hitProg * 3.14159f) * (80.0f + fr * 25.0f) + hitProg * 120.0f;
                    SDL_SetRenderDrawColor(ren, 240, 240, 245, (Uint8)(240 * (1.0f - hitProg)));
                    SDL_Rect fragR = { (int)frX, (int)frY, 8, 8 };
                    SDL_RenderFillRect(ren, &fragR);
                }

                // 4. NUBE DE POLVO Y ONDA DE CHOQUE EXPANDIÉNDOSE DE LAS BOTAS DEL RIVAL
                SDL_SetRenderDrawColor(ren, 220, 210, 190, (Uint8)(240 * (1.0f - hitProg)));
                for (int p = 0; p < 12; ++p) {
                    float dustSpread = (float)(p - 6) * 22.0f * (1.0f + hitProg * 0.8f);
                    float dustLift = std::sin(hitProg * 3.14159f) * (48.0f + (p % 4) * 16.0f);
                    SDL_Rect dustP = { (int)(rivalX + dustSpread), (int)(580 - dustLift), 24, 24 };
                    SDL_RenderFillRect(ren, &dustP);
                }

                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
            }
        }

        // Helper para cajas neo-brutalistas
        auto drawBrutalistBox = [&](int x, int y, int w, int h, bool filled) {
            SDL_SetRenderDrawColor(ren, 10, 10, 10, 255);
            SDL_Rect shadow = {x + 16, y + 16, w, h};
            SDL_RenderFillRect(ren, &shadow);
            
            SDL_SetRenderDrawColor(ren, filled ? 10 : 250, filled ? 10 : 250, filled ? 10 : 250, 255);
            SDL_Rect box = {x, y, w, h};
            SDL_RenderFillRect(ren, &box);
            
            SDL_SetRenderDrawColor(ren, 10, 10, 10, 255);
            for (int b = 0; b < 6; ++b) {
                SDL_Rect border = {x + b, y + b, w - b*2, h - b*2};
                SDL_RenderDrawRect(ren, &border);
            }
        };

        if (state == STATE_WAITING) {
            renderText(ren, font, "DUELO DE VAQUEROS", 960, 60, {10, 10, 10, 255}, true, 1.2f);

            auto drawPlayerText = [&](Player& p, int cx, std::string label) {
                if (!p.gc) return;
                bool correctPose = isMatchingPose(p.accel, p.cal_down, 0.5f);
                SDL_Color textColor = p.ready ? SDL_Color{50, 200, 50, 255} : SDL_Color{10, 10, 10, 255};
                
                renderText(ren, font, label, cx, 180, {10, 10, 10, 255}, true, 0.6f);
                
                std::string status = "APUNTA ABAJO";
                if (p.ready) status = "LISTO";
                else if (correctPose) status = "PULSA X";
                
                drawBrutalistBox(cx - 140, 740, 280, 65, false);
                renderText(ren, font, status, cx, 772, textColor, true, 0.45f);
            };
            
            drawPlayerText(p1, 480, "JUGADOR 1");
            if (p2.gc) drawPlayerText(p2, 1440, "JUGADOR 2");
            else {
                drawBrutalistBox(1440 - 200, 740, 400, 65, false);
                renderText(ren, font, "ESPERANDO JUGADOR 2...", 1440, 772, {100, 100, 100, 255}, true, 0.45f);
            }
        }

        if (targetTex) {
            SDL_SetRenderTarget(ren, NULL);
            SDL_SetRenderDrawColor(ren, 15, 15, 18, 255);
            SDL_RenderClear(ren);

            // CÁMARA CINEMATOGRÁFICA DINÁMICA: Paneo ultra suave con seguimiento balístico y trauma de impacto
            float zoom = 1.0f + (tension_prog * 0.35f);
            float focusX = 960.0f;
            float focusY = 540.0f;
            float traumaShakeX = 0.0f;
            float traumaShakeY = 0.0f;

            if (state == STATE_SHOWDOWN && bullet_winner > 0) {
                Uint32 elapsedShowdown = SDL_GetTicks() - hit_time;
                int dir = (bullet_winner == 1) ? 1 : -1;

                if (elapsedShowdown < 100) {
                    // 1. Frame freeze inicial enfocado en el cañón del tirador
                    focusX = (bullet_winner == 1) ? 755.0f : 1165.0f;
                    focusY = 442.0f;
                    zoom = 1.62f;
                } else if (elapsedShowdown < 1950) {
                    // 2. Seguimiento lateral cinemático acompañando la bala en vuelo
                    float rawP = (float)(elapsedShowdown - 100) / 1850.0f;
                    float smoothP = rawP * rawP * (3.0f - 2.0f * rawP);
                    float bX = (bullet_winner == 1) ? (755.0f + 635.0f * smoothP) : (1165.0f - 635.0f * smoothP);
                    
                    // Empuje de zoom progresivo de 1.65x a 1.82x
                    zoom = 1.65f + rawP * 0.17f;
                    
                    // Composición visual: cámara adelanta ligeramente a la bala para ver el objetivo enfrente
                    float leadOffset = dir * 135.0f * (1.0f - rawP * 0.45f);
                    focusX = bX + leadOffset;
                    focusY = 442.0f + std::sin(elapsedShowdown * 0.003f) * 4.0f;
                } else {
                    // 3. Impacto y K.O.: Enfoque en el rival derrotado cayendo y sombrero volando
                    float hitP = (float)(elapsedShowdown - 1950) / 550.0f;
                    focusX = (bullet_winner == 1) ? 1440.0f : 480.0f;
                    focusY = 505.0f;
                    zoom = 1.82f - hitP * 0.32f; // Se aleja suavemente a 1.50x para captar toda la caída

                    // Trauma de pantalla en el impacto (sacudida violenta que amortigua en 220ms)
                    if (elapsedShowdown < 2170) {
                        float trauma = 1.0f - (float)(elapsedShowdown - 1950) / 220.0f;
                        traumaShakeX = std::sin(elapsedShowdown * 0.22f) * 16.0f * trauma;
                        traumaShakeY = std::cos(elapsedShowdown * 0.28f) * 12.0f * trauma;
                    }
                }
            }

            int w = (int)(1920.0f / zoom);
            int h = (int)(1080.0f / zoom);
            int srcX = (int)(focusX + traumaShakeX - w / 2.0f);
            int srcY = (int)(focusY + traumaShakeY - h / 2.0f);
            if (srcX < 0) srcX = 0;
            if (srcX + w > 1920) srcX = 1920 - w;
            if (srcY < 0) srcY = 0;
            if (srcY + h > 1080) srcY = 1080 - h;

            SDL_Rect src = { srcX, srcY, w, h };
            SDL_RenderCopy(ren, targetTex, &src, NULL);

            // BARRAS CINEMASCOPE 2.39:1 (LETTERBOX) DIBUJADAS EN PANTALLA REAL (NUNCA AFECTADAS POR ZOOM)
            if (state == STATE_TENSION || state == STATE_DRAW || state == STATE_SHOWDOWN) {
                int barHeight = (int)(95.0f * tension_prog);
                if (state == STATE_SHOWDOWN) {
                    Uint32 el = SDL_GetTicks() - hit_time;
                    float expandT = std::min(1.0f, (float)el / 350.0f);
                    barHeight = (int)(95.0f + 40.0f * expandT); // Se expande a 135px para ratio 2.40:1 Cinemascope puro
                }
                SDL_SetRenderDrawColor(ren, 10, 10, 12, 255);
                SDL_Rect topBar = { 0, 0, 1920, barHeight };
                SDL_Rect bottomBar = { 0, 1080 - barHeight, 1920, barHeight };
                SDL_RenderFillRect(ren, &topBar);
                SDL_RenderFillRect(ren, &bottomBar);
            }
        }

        // FASE 1 DEL CLÍMAX: HIT-STOP CON INVERSIÓN DE COLOR NEGRO Y RELÁMPAGO (Primeros 100ms)
        if (state == STATE_SHOWDOWN) {
            Uint32 elapsedShowdown = SDL_GetTicks() - hit_time;
            if (elapsedShowdown < 100) {
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                // Oscurecimiento de alto contraste instantáneo
                SDL_SetRenderDrawColor(ren, 10, 10, 14, 245);
                SDL_Rect fullScreen = { 0, 0, 1920, 1080 };
                SDL_RenderFillRect(ren, &fullScreen);

                // Destello estelar geométrico exactamente en la boca del revólver
                float flashOriginX = (bullet_winner == 1) ? 755.0f : 1165.0f;
                SDL_SetRenderDrawColor(ren, 255, 235, 70, 255);
                SDL_Rect flashSpikeH = { (int)(flashOriginX - 70), 442 - 8, 140, 16 };
                SDL_RenderFillRect(ren, &flashSpikeH);
                SDL_Rect flashSpikeV = { (int)(flashOriginX - 8), 442 - 70, 16, 140 };
                SDL_RenderFillRect(ren, &flashSpikeV);
                
                // Núcleo blanco fulminante
                SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                SDL_Rect flashCore = { (int)(flashOriginX - 25), 442 - 12, 50, 24 };
                SDL_RenderFillRect(ren, &flashCore);

                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
            }
        }

        // INTERFAZ DE RESULTADO DINÁMICA, MODERNA Y ELEGANTE (Renderizada en pantalla limpia sin zoom)
        if (state == STATE_RESULT) {
            // Entrada suave y limpia desde arriba (después de 2500ms de clímax cinematográfico)
            Uint32 resElapsed = SDL_GetTicks() - (hit_time + 2500);
            float animProg = std::min(1.0f, (float)resElapsed / 350.0f);
            float smoothT = animProg * (2.0f - animProg); // Ease-out cuadrático
            int bannerY = (int)(25.0f - (1.0f - smoothT) * 40.0f);

            int cardW = 920;
            int cardH = 190;
            int cardX = 960 - cardW / 2;

            // Sombra brutalista de la tarjeta
            SDL_SetRenderDrawColor(ren, 15, 15, 15, 255);
            SDL_Rect sR = { cardX + 12, bannerY + 12, cardW, cardH };
            SDL_RenderFillRect(ren, &sR);

            // Fondo blanco nítido
            SDL_SetRenderDrawColor(ren, 250, 250, 250, 255);
            SDL_Rect bR = { cardX, bannerY, cardW, cardH };
            SDL_RenderFillRect(ren, &bR);

            // Borde exterior negro grueso brutalista
            SDL_SetRenderDrawColor(ren, 15, 15, 15, 255);
            for (int b = 0; b < 4; ++b) {
                SDL_Rect bor = { cardX + b, bannerY + b, cardW - b * 2, cardH - b * 2 };
                SDL_RenderDrawRect(ren, &bor);
            }

            // Franja superior de acento con color del ganador
            SDL_Color headerCol = (winner == 1) ? SDL_Color{35, 160, 60, 255} : ((winner == 2) ? SDL_Color{210, 45, 45, 255} : SDL_Color{220, 160, 20, 255});
            SDL_SetRenderDrawColor(ren, headerCol.r, headerCol.g, headerCol.b, 255);
            SDL_Rect accentBar = { cardX + 4, bannerY + 4, cardW - 8, 8 };
            SDL_RenderFillRect(ren, &accentBar);

            std::string mainTitle = "";
            std::string subTitle = "";

            if (winner == 1) {
                mainTitle = "¡VICTORIA: JUGADOR 1!";
                if (p2.foul) subTitle = "FALTA: EL JUGADOR 2 SE ADELANTÓ";
                else if (reaction_time_ms > 0) subTitle = "TIEMPO DE REACCIÓN: " + std::to_string(reaction_time_ms) + " MS";
            } else if (winner == 2) {
                mainTitle = "¡VICTORIA: JUGADOR 2!";
                if (p1.foul) subTitle = "FALTA: EL JUGADOR 1 SE ADELANTÓ";
                else if (reaction_time_ms > 0) subTitle = "TIEMPO DE REACCIÓN: " + std::to_string(reaction_time_ms) + " MS";
            } else {
                mainTitle = "¡EMPATE TÉCNICO!";
                subTitle = "DISPARO SIMULTÁNEO";
            }

            // Título perfectamente proporcionado y centrado (escala 0.46f que entra con holgura)
            renderText(ren, font, mainTitle, 960, bannerY + 48, {15, 15, 15, 255}, true, 0.46f);

            // Insignia / Caja pequeña para el tiempo de reacción (bien espaciada)
            if (!subTitle.empty()) {
                int tagW = 460;
                int tagH = 38;
                int tagX = 960 - tagW / 2;
                int tagY = bannerY + 92;

                SDL_SetRenderDrawColor(ren, 240, 240, 242, 255);
                SDL_Rect tagR = { tagX, tagY, tagW, tagH };
                SDL_RenderFillRect(ren, &tagR);
                SDL_SetRenderDrawColor(ren, 25, 25, 25, 255);
                SDL_RenderDrawRect(ren, &tagR);

                SDL_Color subColor = (p1.foul || p2.foul) ? SDL_Color{180, 40, 40, 255} : SDL_Color{30, 95, 185, 255};
                renderText(ren, font, subTitle, 960, tagY + 19, subColor, true, 0.28f);
            }

            // Pie con instrucciones limpias
            renderText(ren, font, "[X] Revancha   •   [O] Salir", 960, bannerY + 158, {90, 90, 90, 255}, true, 0.26f);
        }

        SDL_RenderPresent(ren);

        Uint32 frameElapsed = SDL_GetTicks() - frameStart;
        if (frameElapsed < 16) {
            SDL_Delay(1);
        }
    }

    // Liberar cache de fuentes
    for (auto& pair : g_textCache) {
        if (pair.second) SDL_DestroyTexture(pair.second);
    }
    g_textCache.clear();

    if (font) TTF_CloseFont(font);
    if (whiteTex) SDL_DestroyTexture(whiteTex);
    if (targetTex) SDL_DestroyTexture(targetTex);
    
    if (wav_buffer) SDL_FreeWAV(wav_buffer);
    if (controller_audio > 0) SDL_CloseAudioDevice(controller_audio);
    if (sfxDisparoPC) Mix_FreeChunk(sfxDisparoPC);
    if (bgmTension) Mix_FreeChunk(bgmTension);
    if (sfxHeartbeat) { std::free(sfxHeartbeat->abuf); std::free(sfxHeartbeat); }
    if (sfxTick) { std::free(sfxTick->abuf); std::free(sfxTick); }
    if (sfxBell) { std::free(sfxBell->abuf); std::free(sfxBell); }
    if (sfxHammer) { std::free(sfxHammer->abuf); std::free(sfxHammer); }
    if (sfxTimeWarp) { std::free(sfxTimeWarp->abuf); std::free(sfxTimeWarp); }
    if (sfxBulletFly) { std::free(sfxBulletFly->abuf); std::free(sfxBulletFly); }
    if (sfxImpact) { std::free(sfxImpact->abuf); std::free(sfxImpact); }
    Mix_CloseAudio();

    if (p1.gc) SDL_GameControllerClose(p1.gc);
    if (p2.gc) SDL_GameControllerClose(p2.gc);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
    return 0;
}