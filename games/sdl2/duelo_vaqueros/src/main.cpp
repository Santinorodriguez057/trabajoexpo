#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <unordered_map>

// Estados del juego
enum GameState {
    STATE_WAITING, 
    STATE_TENSION, 
    STATE_DRAW,    
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

void drawCowboy(SDL_Renderer* ren, SDL_Texture* whiteTex, float cx, float cy, int state_phase, bool flip, float t) {
    const float S = 1.35f; // Escala imponente para que los vaqueros tengan gran presencia
    float breathe = (state_phase == 2) ? (sin(t * 1.5f) * 1.5f) : (sin(t * 3.5f) * 4.0f);
    float shake = (state_phase == 1) ? (sin(t * 55.0f) * 3.0f) : 0.0f;

    SDL_RendererFlip flipFlag = flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    int dir = flip ? -1 : 1;

    // Helper limpio para dibujar partes con borde negro nítido (3-4px) estilo entintado comic / neo-brutalismo
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

    SDL_Color cWhite      = {255, 255, 255, 255};
    SDL_Color cBackLimb   = {220, 220, 222, 255}; // Tono sutil para extremidades traseras (profundidad limpia)
    SDL_Color cBandana    = {45, 45, 48, 255};     // Pañuelo gris carbón
    SDL_Color cHatBand    = {35, 35, 38, 255};     // Cinta del sombrero
    SDL_Color cFaceShadow = {50, 50, 55, 255};     // Sombra bajo el ala del sombrero
    SDL_Color cBlack      = {18, 18, 18, 255};
    SDL_Color cSilver     = {235, 235, 235, 255};
    SDL_Color cHolster    = {55, 55, 58, 255};
    SDL_Color cFlash      = {255, 235, 80, 255};

    // =========================================================================
    // CASO 1: VAQUERO DERROTADO (Fase 3) - Cae de espaldas en el suelo
    // =========================================================================
    if (state_phase == 3) {
        cWhite = {195, 195, 195, 255};
        cBackLimb = {165, 165, 165, 255};

        // Sombra alargada en el suelo
        drawR(0, 155, 220, 16, 0.0f, cBlack, false);

        // Piernas estiradas en el piso
        drawR(45, 140, 85, 26, 0.0f, cWhite, true, 3);
        // Botas caídas
        drawR(88, 138, 38, 24, 0.0f, cWhite, true, 3);
        drawR(90, 148, 40, 5, 0.0f, cBlack, false);

        // Torso plano en el suelo
        drawR(-20, 136, 75, 46, 0.0f, cWhite, true, 3);
        // Cinturón caído
        drawR(12, 136, 16, 48, 0.0f, cBlack, true, 2);
        drawR(12, 136, 16, 18, 0.0f, cSilver, true, 2);

        // Pañuelo
        drawR(-50, 136, 20, 30, 0.0f, cBandana, true, 2);

        // Cabeza en el suelo
        drawR(-78, 134, 46, 44, 0.0f, cWhite, true, 3);
        // Ojos de "X" de derrota
        drawR(-76, 132, 12, 3, 45.0f, cBlack, false);
        drawR(-76, 132, 12, 3, -45.0f, cBlack, false);

        // Sombrero volado y tirado a un costado
        drawR(-120, 142, 135, 12, 22.0f, cWhite, true, 3);
        drawR(-116, 124, 50, 34, 22.0f, cWhite, true, 3);
        drawR(-118, 136, 50, 12, 22.0f, cHatBand, true, 2);

        // Revólver caído en el suelo
        drawR(110, 146, 38, 14, -15.0f, cWhite, true, 2);
        drawR(96, 150, 14, 22, 25.0f, cWhite, true, 2);
        return;
    }

    // =========================================================================
    // VAQUERO EN PIE (Fases 0, 1, 2, 4)
    // =========================================================================
    float bodyAngle = 0.0f;
    float frontArmAngle = 10.0f;
    float backArmAngle = 5.0f;
    float headTilt = 0.0f;

    if (state_phase == 1) { // Tensión (listo para desenfundar)
        bodyAngle = 6.0f;
        frontArmAngle = -35.0f; // Mano en la funda
        backArmAngle = -10.0f;
    } else if (state_phase == 2) { // Disparo / Quick-draw
        bodyAngle = -4.0f; // Firme con retroceso
        frontArmAngle = -90.0f; // Brazo extendido horizontal apuntando al rival
        backArmAngle = 30.0f; // Brazo trasero hacia atrás equilibrando
    } else if (state_phase == 4) { // Falta (cabizbajo)
        bodyAngle = 12.0f;
        headTilt = 15.0f;
        frontArmAngle = 45.0f;
        backArmAngle = 20.0f;
    }

    float torsoY = -5.0f + breathe + shake;
    float headY  = -86.0f + breathe + shake;

    // 1. SOMBRA DE SUELO (Ancla al personaje firmemente sobre el terreno)
    drawR(2, 155, 140, 14, 0.0f, cBlack, false);

    // 2. BRAZO TRASERO (Capa de fondo - enmarca la silueta exteriormente como en la referencia)
    if (state_phase == 2) {
        // En disparo: brazo doblado hacia atrás equilibrando la postura (estilo cómic)
        drawR(-20, torsoY - 4, 18, 44, bodyAngle + 20.0f, cBackLimb, true, 3);
        drawR(-14, torsoY + 30, 16, 42, bodyAngle - 28.0f, cBackLimb, true, 3);
        drawR(-8, torsoY + 52, 16, 16, bodyAngle - 10.0f, cBackLimb, true, 2); // Puño en la cadera trasera
    } else if (state_phase == 1) {
        // En tensión: brazo trasero retraído y tenso
        drawR(-30, torsoY - 6, 18, 46, bodyAngle - 22.0f, cBackLimb, true, 3);
        drawR(-38, torsoY + 30, 16, 42, bodyAngle - 6.0f, cBackLimb, true, 3);
        drawR(-38, torsoY + 58, 16, 16, bodyAngle - 6.0f, cBackLimb, true, 2); // Puño tenso
    } else {
        // En reposo (Fase 0 y 4): brazo cuelga al costado exterior con sutil flexión natural de codo
        drawR(-34, torsoY + 4, 18, 46, bodyAngle - 10.0f, cBackLimb, true, 3);
        drawR(-38, torsoY + 42, 16, 46, bodyAngle + 2.0f, cBackLimb, true, 3);
        drawR(-38, torsoY + 62, 16, 4, bodyAngle + 2.0f, cBlack, false);        // Dobladillo puño
        drawR(-38, torsoY + 74, 16, 18, bodyAngle + 2.0f, cBackLimb, true, 2); // Mano relajada
    }

    // 3. PIERNAS Y BOTAS EN POSTURA ANCHA (A) - Firmes, rectas y sólidas estilo cómic (sin flexiones raras)
    // Pierna trasera (Pantalón ancho y aplomado)
    drawR(-24, 95, 34, 96, -3.0f, cBackLimb, true, 3);
    drawR(-24, 95, 24, 3, -3.0f, cBlack, false);       // Pliegue cómic de rodilla
    drawR(-28, 145, 48, 20, 0.0f, cBackLimb, true, 3); // Bota trasera
    drawR(-46, 149, 12, 10, 0.0f, cBlack, false);      // Tacón trasero
    drawR(-26, 153, 52, 4, 0.0f, cBlack, false);       // Suela bota trasera

    // Pierna delantera (Pantalón ancho y firme hacia adelante)
    drawR(24, 95, 36, 96, 4.0f, cWhite, true, 3);
    drawR(10, 95, 10, 92, 4.0f, cBackLimb, false);     // Sombra lateral interior
    drawR(26, 95, 26, 3, 4.0f, cBlack, false);        // Pliegue cómic de rodilla
    drawR(30, 145, 52, 20, 0.0f, cWhite, true, 3);     // Bota delantera
    drawR(10, 149, 12, 10, 0.0f, cBlack, false);       // Tacón delantero
    drawR(50, 147, 16, 14, 0.0f, cWhite, true, 2);     // Puntera angular hacia el rival
    drawR(32, 153, 56, 4, 0.0f, cBlack, false);        // Suela bota delantera

    // 4. PELVIS Y CADERA (Base de apoyo sólida y nivelada)
    drawR(0, 48, 60, 26, 0.0f, cWhite, true, 3);

    // 5. TORSO HEROICO CON V-TAPER (Pecho ancho en hombros, cintura estrecha estilo cómic)
    // Contorno exterior unificado
    drawR(0, torsoY - 10, 68, 54, bodyAngle, cBlack, false);
    drawR(0, torsoY + 20, 52, 52, bodyAngle, cBlack, false);

    // Relleno blanco continuo (silueta atlética que abre espacio para los brazos al costado)
    drawR(0, torsoY - 10, 62, 48, bodyAngle, cWhite, false);
    drawR(0, torsoY + 20, 46, 48, bodyAngle, cWhite, false);

    // Sombreado celular en el lateral de la espalda
    drawR(-20, torsoY + 4, 10, 80, bodyAngle, cBackLimb, false);
    // Pliegue central sutil de la camisa vaquera
    drawR(4, torsoY + 8, 3, 38, bodyAngle, cBlack, false);

    // 6. FUNDA (Holster) en la cadera delantera
    drawR(24, torsoY + 60, 20, 46, bodyAngle + 6.0f, cHolster, true, 3);

    // Si el revólver está enfundado (Fases 0, 1, 4), dibujamos la empuñadura asomándose
    if (state_phase != 2) {
        drawR(22, torsoY + 42, 14, 26, bodyAngle - 20.0f, cWhite, true, 2);
        drawR(28, torsoY + 36, 6, 8, bodyAngle - 20.0f, cBlack, false); // Martillo
    }

    // 7. CINTURÓN Y HEBILLA BRUTALISTA (Cintura definida)
    drawR(0, torsoY + 38, 56, 16, bodyAngle, cBlack, true, 2);
    // Hebilla metálica plateada
    drawR(2, torsoY + 38, 20, 18, bodyAngle, cSilver, true, 2);
    drawR(2, torsoY + 38, 8, 8, bodyAngle, cBlack, false);
    // Bolsita / cartuchera lateral
    drawR(-20, torsoY + 38, 10, 16, bodyAngle, cHatBand, true, 2);

    // 8. CUELLO Y PAÑUELO (Bandana triangular icónica de la referencia)
    drawR(2, headY + 34, 26, 18, bodyAngle, cWhite, false);
    // Banda del pañuelo en el cuello
    drawR(2, headY + 30, 42, 15, bodyAngle, cBandana, true, 2);
    // Pico triangular frontal que cae sobre el pecho
    drawR(14, headY + 42, 28, 28, bodyAngle + 45.0f, cBandana, true, 2);
    drawR(12, headY + 42, 18, 18, bodyAngle + 45.0f, {35, 35, 38, 255}, false);

    // 9. CABEZA Y CARA
    drawR(4, headY, 46, 46, bodyAngle + headTilt, cWhite, true, 3);
    // Sombra misteriosa bajo el ala del sombrero
    drawR(4, headY - 11, 44, 22, bodyAngle + headTilt, cFaceShadow, false);
    // Dos ojos minimalistas sutiles
    drawR(14, headY + 2, 5, 5, bodyAngle + headTilt, cBlack, false);
    drawR(-4, headY + 2, 5, 5, bodyAngle + headTilt, cBlack, false);

    // 10. SOMBRERO (Ala súper ancha y copa con cinta oscura)
    // Ala horizontal ancha
    drawR(4, headY - 24, 158, 13, bodyAngle + headTilt, cWhite, true, 3);
    drawR(4, headY - 18, 154, 4, bodyAngle + headTilt, cBlack, false); // Borde inferior ala
    // Copa rectangular con hendidura superior sutil
    drawR(4, headY - 50, 56, 40, bodyAngle + headTilt, cWhite, true, 3);
    // Cinta oscura de la copa
    drawR(4, headY - 35, 56, 13, bodyAngle + headTilt, cHatBand, true, 2);
    drawR(4, headY - 35, 14, 11, bodyAngle + headTilt, cSilver, true, 1); // Hebilla del sombrero

    // 11. BRAZO DELANTERO Y ACCIÓN DE LA MANO Y REVÓLVER
    if (state_phase == 0) {
        // EN REPOSO / ESPERA: Brazo exterior limpio con sutil flexión natural de codo (estilo ilustración de referencia)
        // PASO 1: Contorno exterior
        drawR(36, torsoY + 4, 26, 46, bodyAngle + 10.0f, cBlack, false);
        drawR(40, torsoY + 42, 24, 46, bodyAngle - 2.0f, cBlack, false);

        // PASO 2: Relleno blanco continuo (manga unificada sin costura)
        drawR(36, torsoY + 4, 20, 42, bodyAngle + 10.0f, cWhite, false);
        drawR(40, torsoY + 42, 18, 42, bodyAngle - 2.0f, cWhite, false);

        // PASO 3: Puño y mano relajada
        drawR(40, torsoY + 62, 18, 4, bodyAngle - 2.0f, cBackLimb, false);  // Dobladillo puño
        drawR(40, torsoY + 74, 16, 20, bodyAngle - 2.0f, cWhite, true, 2);  // Mano geométrica
        drawR(44, torsoY + 71, 5, 9, bodyAngle + 10.0f, cWhite, true, 1);   // Pulgar
        drawR(40, torsoY + 82, 14, 4, bodyAngle - 2.0f, cBlack, false);     // Sombra dedos
        drawR(40, torsoY + 77, 10, 2, bodyAngle - 2.0f, cBlack, false);     // Nudillos
    } else if (state_phase == 1) {
        // EN TENSIÓN (DUELO / ACECHO): Codo afilado hacia afuera (>) y mano acechando la cartuchera
        // PASO 1: Contorno perimetral unificado en negro
        drawR(38, torsoY - 8, 26, 46, bodyAngle + 22.0f, cBlack, false);
        drawR(36, torsoY + 26, 24, 44, bodyAngle - 22.0f, cBlack, false);

        // PASO 2: Relleno blanco continuo (sin corte en el codo)
        drawR(38, torsoY - 8, 20, 42, bodyAngle + 22.0f, cWhite, false);
        drawR(36, torsoY + 26, 18, 40, bodyAngle - 22.0f, cWhite, false);

        // PASO 3: Detalles y mano acechando la cartuchera
        drawR(46, torsoY + 9, 6, 2, bodyAngle + 22.0f, cBlack, false);      // Pliegue en el codo
        drawR(28, torsoY + 44, 18, 4, bodyAngle - 22.0f, cBackLimb, false); // Dobladillo puño

        // Mano en garra tensa acechando justo sobre la empuñadura
        drawR(24, torsoY + 46, 16, 15, bodyAngle - 15.0f, cWhite, true, 2); // Palma arqueada
        drawR(20, torsoY + 40, 6, 8, bodyAngle - 25.0f, cWhite, true, 1);   // Pulgar tenso
        drawR(27, torsoY + 53, 12, 10, bodyAngle + 15.0f, cWhite, true, 1); // Dedos listos para atenazar
        drawR(27, torsoY + 54, 8, 2, bodyAngle + 15.0f, cBlack, false);    // Nudillos
    } else if (state_phase == 4) {
        // Falta: brazo cuelga derrotado
        drawR(36, torsoY + 38, 20, 74, bodyAngle + 35.0f, cWhite, true, 3);
        drawR(44, torsoY + 76, 16, 16, bodyAngle + 35.0f, cWhite, true, 2);
    } else if (state_phase == 2) {
        // DISPARAR / QUICK-DRAW: Manga horizontal continua a la altura del hombro (ritmo más pausado y con peso)
        float recoilCycle = fmod(t * 2.5f, 1.0f); // Velocidad moderada y cinematográfica
        float recoilKick = (recoilCycle < 0.30f) ? (1.0f - recoilCycle / 0.30f) : 0.0f;
        float recoilAngle = -recoilKick * 12.0f; // Salto del cañón hacia arriba
        float recoilX = -recoilKick * 5.0f;     // Retroceso horizontal del brazo

        // Sombra de volumen inferior continua bajo todo el brazo
        drawR(58 + recoilX, torsoY - 12, 78, 6, recoilAngle * 0.4f, cBlack, false);

        // Manga horizontal continua de hombro a muñeca (a nivel de hombro torsoY - 22)
        drawR(58 + recoilX, torsoY - 22, 78, 22, recoilAngle * 0.4f, cWhite, true, 3);

        // Dobladillo puño en la muñeca
        drawR(96 + recoilX, torsoY - 22, 4, 22, recoilAngle * 0.4f, cBackLimb, false);

        // REVÓLVER BRUTALISTA CON RETROCESO COMPLETO Y MANO ARTICULADA
        float gunBaseX = 106.0f + recoilX;
        float gunBaseY = torsoY - 22.0f;
        float gunRot = recoilAngle;

        auto drawGunPart = [&](float relX, float relY, float w, float h, float partRot, SDL_Color col, bool outline = true, int bW = 2) {
            float rad = gunRot * 3.14159f / 180.0f;
            float rx = relX * cos(rad) - relY * sin(rad);
            float ry = relX * sin(rad) + relY * cos(rad);
            drawR(gunBaseX + rx, gunBaseY + ry, w, h, gunRot + partRot, col, outline, bW);
        };

        // Empuñadura angulada
        drawGunPart(6, 14, 16, 28, 20.0f, cWhite, true, 2);
        drawGunPart(8, 26, 14, 5, 20.0f, cBlack, false);
        // Martillo
        drawGunPart(2, -10, 8, 10, -15.0f, cBlack, false);
        // Guardamonte
        drawGunPart(16, 8, 14, 9, 0.0f, cBlack, true, 1);
        // Armazón
        drawGunPart(18, -3, 22, 18, 0.0f, cWhite, true, 2);
        // Tambor rectangular con estrías negras
        drawGunPart(32, -3, 26, 20, 0.0f, cWhite, true, 2);
        drawGunPart(32, -7, 20, 3, 0.0f, cBlack, false);
        drawGunPart(32, 1, 20, 3, 0.0f, cBlack, false);
        // Cañón largo y recto
        drawGunPart(62, -5, 42, 13, 0.0f, cWhite, true, 2);
        // Guía inferior del cañón
        drawGunPart(52, 3, 22, 5, 0.0f, cBlack, false);
        // Mirilla frontal en la punta
        drawGunPart(80, -13, 6, 6, 0.0f, cBlack, false);

        // MANO ARTICULADA EMPUÑANDO EL REVÓLVER
        drawGunPart(0, 0, 16, 18, 0.0f, cWhite, true, 2);                // Dorso de la mano
        drawGunPart(2, -7, 8, 8, -20.0f, cWhite, true, 1);               // Pulgar sobre el armazón
        drawGunPart(9, 12, 10, 20, 15.0f, cWhite, true, 1);              // Dedos cerrados sobre la culata
        drawGunPart(9, 9, 7, 2, 15.0f, cBlack, false);                   // Línea de nudillos
        drawGunPart(9, 15, 7, 2, 15.0f, cBlack, false);                  // Línea de nudillos
        drawGunPart(13, 2, 9, 4, 0.0f, cWhite, false);                   // Dedo índice al gatillo

        // CASQUILLO DORADO EXPULSADO (Sincronizado con cada disparo, movimiento suave)
        float shellProgress = recoilCycle;
        float shellX = gunBaseX + 28.0f - shellProgress * 42.0f;
        float shellY = gunBaseY - 12.0f - sin(shellProgress * 3.14159f) * 36.0f + shellProgress * 14.0f;
        float shellRot = shellProgress * 360.0f;
        drawR(shellX, shellY, 11, 6, shellRot, {245, 210, 50, 255}, true, 1);

        // FOGONAZO BRUTALISTA Y HUMO
        if (recoilKick > 0.40f) {
            float rad = gunRot * 3.14159f / 180.0f;
            float mx = 94.0f * cos(rad) - (-5.0f) * sin(rad);
            float my = 94.0f * sin(rad) + (-5.0f) * cos(rad);
            float muzzleX = gunBaseX + mx;
            float muzzleY = gunBaseY + my;

            // Estrella geométrica de 8 puntas
            drawR(muzzleX + 16, muzzleY, 44, 44, gunRot, cFlash, true, 2);
            drawR(muzzleX + 16, muzzleY, 44, 44, gunRot + 45.0f, cFlash, true, 2);
            drawR(muzzleX + 16, muzzleY, 20, 20, gunRot, cWhite, false);

            // Nubes de humo saliendo del cañón
            drawR(muzzleX + 46, muzzleY - 8, 14, 14, gunRot + 20.0f, {225, 225, 225, 255}, true, 1);
            drawR(muzzleX + 66, muzzleY + 2, 22, 22, gunRot + 35.0f, {195, 195, 195, 255}, true, 1);
            drawR(muzzleX + 88, muzzleY - 4, 18, 18, gunRot + 15.0f, {175, 175, 175, 255}, true, 1);
        }
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

void drawDesertBackground(SDL_Renderer* ren, SDL_Texture* whiteTex, float t, Tumbleweed* weeds, int max_weeds, int state) {
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

    // Dibujar formas planas sin bordes gruesos para no robar atencion
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
    
    // Nubes
    drawFlat(200, 150, 150, 30, 0, cloudCol);
    drawFlat(220, 120, 80, 30, 0, cloudCol);
    drawFlat(1600, 250, 200, 40, 0, cloudCol);
    drawFlat(1650, 210, 100, 40, 0, cloudCol);
    drawFlat(800, 180, 120, 30, 0, cloudCol);

    // Sol Centrado
    float sunX = 960;
    float sunY = 320;
    drawFlat(sunX, sunY, 160, 160, 0, sunCol);
    drawFlat(sunX, sunY, 160, 160, 45, sunCol);
    for (int i = 0; i < 12; i++) {
        float a = i * 30.0f;
        float rad = a * 3.14159f / 180.0f;
        drawFlat(sunX + cos(rad)*110, sunY + sin(rad)*110, 40, 15, a, sunCol);
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
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
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

    auto playFireSound = [&]() {
        if (sfxDisparoPC) {
            Mix_PlayChannel(-1, sfxDisparoPC, 0); // Siempre por la PC
        }
        if (controller_audio > 0 && wav_buffer != nullptr) {
            SDL_ClearQueuedAudio(controller_audio);
            SDL_QueueAudio(controller_audio, wav_buffer, wav_length);
            SDL_PauseAudioDevice(controller_audio, 0); // Y tambien por el mando si se puede
        }
    };

    Player p1, p2;
    GameState state = STATE_WAITING;
    Uint32 tension_start_time = 0;
    Uint32 tension_duration = 0;
    
    int winner = 0; // 0: nadie, 1: J1, 2: J2

    // Inicialización y asignación eficiente de mandos (sin escanear sysfs cada frame)
    auto setupController = [](Player& p, SDL_GameController* gc) {
        p.gc = gc;
        p.ready = false;
        p.foul = false;
        p.has_drawn = false;
        p.has_gyro = SDL_GameControllerHasSensor(gc, SDL_SENSOR_GYRO);
        p.has_accel = SDL_GameControllerHasSensor(gc, SDL_SENSOR_ACCEL);
        if (p.has_gyro) SDL_GameControllerSetSensorEnabled(gc, SDL_SENSOR_GYRO, SDL_TRUE);
        if (p.has_accel) SDL_GameControllerSetSensorEnabled(gc, SDL_SENSOR_ACCEL, SDL_TRUE);
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

    // Detección inicial una sola vez
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
            } else if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
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
                    running = false; // Circulo para salir
                }
                
                if (state == STATE_WAITING && ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                    SDL_JoystickID id = ev.cbutton.which;
                    SDL_JoystickID id1 = p1.gc ? SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(p1.gc)) : -1;
                    SDL_JoystickID id2 = p2.gc ? SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(p2.gc)) : -1;
                    
                    if (id == id1 && isMatchingPose(p1.accel, p1.cal_down, 0.5f)) p1.ready = true;
                    if (id == id2 && isMatchingPose(p2.accel, p2.cal_down, 0.5f)) p2.ready = true;
                }
                
                if (state == STATE_RESULT && ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                    state = STATE_WAITING;
                    p1.ready = p2.ready = p1.foul = p2.foul = false;
                    winner = 0;
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
            // Movimiento brusco del giroscopio (Subimos de 2.0 a 4.0 para que no sea tan sensible)
            if (std::abs(p.gyro[0]) > 4.0f || std::abs(p.gyro[1]) > 4.0f || std::abs(p.gyro[2]) > 4.0f) {
                p.has_drawn = true;
            }
            // O si la gravedad coincide con la posicion apuntando al frente
            // Tolerancia a 0.8f (antes 0.5f) exige que el mando esté mucho más cerca de la horizontal ideal
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

        if (state == STATE_WAITING) {
            // Reiniciamos el estado de desenfunde mientras esperamos
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
                tension_duration = 2000 + (std::rand() % 4000);
                if (bgmTension) Mix_PlayChannel(1, bgmTension, -1);
            }
        }
        else if (state == STATE_TENSION) {
            checkDrawn(p1);
            checkDrawn(p2);
            
            // En tension, moverse (desenfundar) o disparar es falta
            bool p1_foul = p1.has_drawn || hasPulledTrigger(p1);
            bool p2_foul = p2.has_drawn || hasPulledTrigger(p2);
            
            if (p1_foul || p2_foul) {
                playFireSound();
                Mix_HaltChannel(1);
                state = STATE_RESULT;
                if (p1_foul) { p1.foul = true; winner = 2; }
                else { p2.foul = true; winner = 1; }
            } else if (SDL_GetTicks() - tension_start_time > tension_duration) {
                state = STATE_DRAW;
                if (p1.gc) SDL_GameControllerRumble(p1.gc, 0xFFFF, 0xFFFF, 400);
                if (p2.gc) SDL_GameControllerRumble(p2.gc, 0xFFFF, 0xFFFF, 400);
                Mix_HaltChannel(1);
            }
        }
        else if (state == STATE_DRAW) {
            checkDrawn(p1);
            checkDrawn(p2);
            
            // Para disparar, TIENE que haber desenfundado (has_drawn) Y apretar gatillo
            bool p1_fire = p1.has_drawn && hasPulledTrigger(p1);
            bool p2_fire = p2.has_drawn && hasPulledTrigger(p2);
            
            if (p1_fire && !p2_fire) {
                playFireSound(); winner = 1; state = STATE_RESULT;
            } else if (p2_fire && !p1_fire) {
                playFireSound(); winner = 2; state = STATE_RESULT;
            } else if (p1_fire && p2_fire) {
                playFireSound(); winner = 3; state = STATE_RESULT; 
            }
        }

        // --- RENDERIZADO NEO-BRUTALISTA ---
        float currTime = SDL_GetTicks() * 0.001f;
        drawDesertBackground(ren, whiteTex, currTime, tumbleweeds, 8, state);

        // Determinar fases de animacion de los vaqueros
        int phaseP1 = 0;
        int phaseP2 = 0;
        
        if (state == STATE_WAITING) {
            phaseP1 = 0; phaseP2 = 0;
        } else if (state == STATE_TENSION) {
            phaseP1 = 1; phaseP2 = 1;
        } else if (state == STATE_DRAW) {
            phaseP1 = p1.has_drawn ? 2 : 1; 
            phaseP2 = p2.has_drawn ? 2 : 1;
        } else if (state == STATE_RESULT) {
            if (p1.foul) { phaseP1 = 4; phaseP2 = 0; }
            else if (p2.foul) { phaseP1 = 0; phaseP2 = 4; }
            else if (winner == 1) { phaseP1 = 2; phaseP2 = 3; }
            else if (winner == 2) { phaseP1 = 3; phaseP2 = 2; }
            else if (winner == 3) { phaseP1 = 3; phaseP2 = 3; }
        }

        // Dibujar Vaqueros Brutalistas
        drawCowboy(ren, whiteTex, 480, 490, phaseP1, false, currTime);
        drawCowboy(ren, whiteTex, 1440, 490, phaseP2, true, currTime);

        // Utilidad para dibujar cajas brutalistas
        auto drawBrutalistBox = [&](int x, int y, int w, int h, bool filled) {
            // Sombra
            SDL_SetRenderDrawColor(ren, 10, 10, 10, 255);
            SDL_Rect shadow = {x + 16, y + 16, w, h};
            SDL_RenderFillRect(ren, &shadow);
            
            // Fondo
            SDL_SetRenderDrawColor(ren, filled ? 10 : 250, filled ? 10 : 250, filled ? 10 : 250, 255);
            SDL_Rect box = {x, y, w, h};
            SDL_RenderFillRect(ren, &box);
            
            // Borde
            SDL_SetRenderDrawColor(ren, 10, 10, 10, 255);
            for (int b = 0; b < 6; ++b) {
                SDL_Rect border = {x + b, y + b, w - b*2, h - b*2};
                SDL_RenderDrawRect(ren, &border);
            }
        };

        if (state == STATE_WAITING) {
            renderText(ren, font, "DUELO DE VAQUEROS", 960, 80, {10, 10, 10, 255}, true, 1.2f);
            
            auto drawPlayerText = [&](Player& p, int cx, std::string label) {
                if (!p.gc) return;
                bool correctPose = isMatchingPose(p.accel, p.cal_down, 0.5f);
                SDL_Color textColor = p.ready ? SDL_Color{50, 200, 50, 255} : SDL_Color{10, 10, 10, 255};
                
                renderText(ren, font, label, cx, 140, {10, 10, 10, 255}, true, 0.7f);
                
                std::string status = "APUNTA ABAJO";
                if (p.ready) status = "LISTO";
                else if (correctPose) status = "PULSA X";
                
                // Dibujar caja de fondo para el texto debajo de los pies
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
        else if (state == STATE_TENSION) {
            // Sin texto, los vaqueros hablan por si solos
        }
        else if (state == STATE_DRAW) {
            // Flashbang gigante visual para reaccionar (pero sin texto)
        }
        else if (state == STATE_RESULT) {
            // Tarjeta central brutalista para el resultado (mas arriba para ver los cuerpos)
            drawBrutalistBox(460, 80, 1000, 260, false);
            
            std::string mainText = "";
            std::string subText = "";
            
            if (winner == 1) {
                mainText = "¡GANA J1!";
                if (p2.foul) subText = "EL JUGADOR 2 DISPARO ANTES (FALTA)";
            } else if (winner == 2) {
                mainText = "¡GANA J2!";
                if (p1.foul) subText = "EL JUGADOR 1 DISPARO ANTES (FALTA)";
            } else {
                mainText = "¡EMPATE!";
            }
            
            renderText(ren, font, mainText, 960, 170, {10, 10, 10, 255}, true, 2.0f);
            if (!subText.empty()) renderText(ren, font, subText, 960, 260, {80, 80, 80, 255}, true, 0.4f);
            
            renderText(ren, font, "X: Revancha  |  Circulo: Salir", 960, 310, {10, 10, 10, 255}, true, 0.4f);
        }

        SDL_RenderPresent(ren);

        // VSync ya sincroniza a la tasa de refresco del monitor (60 Hz).
        // Si el render termina rápido y VSync no bloqueó, este delay mínimo protege la CPU sin generar parones
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
    
    if (wav_buffer) SDL_FreeWAV(wav_buffer);
    if (controller_audio > 0) SDL_CloseAudioDevice(controller_audio);
    if (sfxDisparoPC) Mix_FreeChunk(sfxDisparoPC);
    if (bgmTension) Mix_FreeChunk(bgmTension);
    Mix_CloseAudio();

    if (p1.gc) SDL_GameControllerClose(p1.gc);
    if (p2.gc) SDL_GameControllerClose(p2.gc);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
    return 0;
}