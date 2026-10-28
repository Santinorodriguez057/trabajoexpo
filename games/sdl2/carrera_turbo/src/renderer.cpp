#include "renderer.h"
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <vector>

namespace {
    constexpr float kPi = 3.1415926535f;

    SDL_Color lerpColor(SDL_Color a, SDL_Color b, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return {
            static_cast<Uint8>(a.r + (b.r - a.r) * t),
            static_cast<Uint8>(a.g + (b.g - a.g) * t),
            static_cast<Uint8>(a.b + (b.b - a.b) * t),
            static_cast<Uint8>(a.a + (b.a - a.a) * t)
        };
    }

    SDL_Color scaleColor(SDL_Color c, float factor) {
        return {
            static_cast<Uint8>(std::clamp(c.r * factor, 0.0f, 255.0f)),
            static_cast<Uint8>(std::clamp(c.g * factor, 0.0f, 255.0f)),
            static_cast<Uint8>(std::clamp(c.b * factor, 0.0f, 255.0f)),
            c.a
        };
    }
}

Renderer::Renderer(SDL_Renderer* ren, TTF_Font* fontTitle, TTF_Font* fontHud, TTF_Font* fontSmall)
    : ren_(ren), fontTitle_(fontTitle), fontHud_(fontHud), fontSmall_(fontSmall) {
    // Textura 1x1 blanca
    whiteTex_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, 1, 1);
    if (whiteTex_) {
        Uint32 pixel = 0xFFFFFFFF;
        SDL_UpdateTexture(whiteTex_, nullptr, &pixel, sizeof(Uint32));
    }
}

Renderer::~Renderer() {
    if (whiteTex_) SDL_DestroyTexture(whiteTex_);
}

void Renderer::drawNeobrutalRect(int x, int y, int w, int h, SDL_Color fill, int borderW, int shadowOffset) {
    if (w <= 0 || h <= 0) return;

    // 1. Sombra dura desplazada sin difuminar (negro sólido)
    if (shadowOffset > 0) {
        SDL_Rect shadowR = {x + shadowOffset, y + shadowOffset, w, h};
        SDL_SetRenderDrawColor(ren_, 18, 18, 18, 255);
        SDL_RenderFillRect(ren_, &shadowR);
    }

    // 2. Borde exterior negro grueso
    if (borderW > 0) {
        SDL_Rect borderR = {x - borderW, y - borderW, w + borderW * 2, h + borderW * 2};
        SDL_SetRenderDrawColor(ren_, 18, 18, 18, 255);
        SDL_RenderFillRect(ren_, &borderR);
    }

    // 3. Relleno principal de color saturado
    SDL_Rect mainR = {x, y, w, h};
    SDL_SetRenderDrawColor(ren_, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(ren_, &mainR);
}

void Renderer::drawNeobrutalCircle(int cx, int cy, int radius, SDL_Color fill, int borderW, int shadowOffset) {
    if (radius <= 0) return;

    // Círculo optimizado en hardware usando abanico de triángulos (SDL_RenderGeometry)
    auto fillCircleFast = [&](int px, int py, int r, SDL_Color col) {
        if (r <= 0) return;
        const int segments = std::clamp(r / 2 + 12, 16, 48);
        std::vector<SDL_Vertex> verts(segments + 1);
        std::vector<int> indices(segments * 3);

        verts[0].position = {static_cast<float>(px), static_cast<float>(py)};
        verts[0].color = col;
        verts[0].tex_coord = {0.0f, 0.0f};

        for (int i = 0; i < segments; ++i) {
            float ang = (static_cast<float>(i) / segments) * 2.0f * kPi;
            verts[i + 1].position = {
                px + std::cos(ang) * r,
                py + std::sin(ang) * r
            };
            verts[i + 1].color = col;
            verts[i + 1].tex_coord = {0.0f, 0.0f};

            indices[i * 3 + 0] = 0;
            indices[i * 3 + 1] = i + 1;
            indices[i * 3 + 2] = (i + 1 == segments) ? 1 : (i + 2);
        }
        SDL_RenderGeometry(ren_, nullptr, verts.data(), static_cast<int>(verts.size()), indices.data(), static_cast<int>(indices.size()));
    };

    if (shadowOffset > 0) {
        fillCircleFast(cx + shadowOffset, cy + shadowOffset, radius + borderW, {18, 18, 18, 255});
    }
    if (borderW > 0) {
        fillCircleFast(cx, cy, radius + borderW, {18, 18, 18, 255});
    }
    fillCircleFast(cx, cy, radius, fill);
}

void Renderer::drawText(TTF_Font* font, const std::string& text, int x, int y, SDL_Color color, bool center) {
    if (!font || text.empty()) return;

    // Contorno nítido de alto contraste de 1px en 4 direcciones (sin sombras borrosas desplazadas)
    SDL_Surface* surfShadow = TTF_RenderUTF8_Blended(font, text.c_str(), {18, 18, 18, 255});
    if (surfShadow) {
        SDL_Texture* texS = SDL_CreateTextureFromSurface(ren_, surfShadow);
        SDL_FreeSurface(surfShadow);
        if (texS) {
            int w, h;
            SDL_QueryTexture(texS, nullptr, nullptr, &w, &h);
            int baseX = center ? x - w / 2 : x;
            const int offsets[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
            for (const auto& off : offsets) {
                SDL_Rect dstS = {baseX + off[0], y + off[1], w, h};
                SDL_RenderCopy(ren_, texS, nullptr, &dstS);
            }
            SDL_DestroyTexture(texS);
        }
    }

    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren_, surf);
    SDL_FreeSurface(surf);
    if (!tex) return;

    int w, h;
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    SDL_Rect dst = {center ? x - w / 2 : x, y, w, h};
    SDL_RenderCopy(ren_, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

void Renderer::renderPolygon(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, SDL_Color color) {
    SDL_Vertex verts[4];
    verts[0].position = { static_cast<float>(x1), static_cast<float>(y1) };
    verts[0].color = color;
    verts[0].tex_coord = {0, 0};
    verts[1].position = { static_cast<float>(x2), static_cast<float>(y2) };
    verts[1].color = color;
    verts[1].tex_coord = {0, 0};
    verts[2].position = { static_cast<float>(x3), static_cast<float>(y3) };
    verts[2].color = color;
    verts[2].tex_coord = {0, 0};
    verts[3].position = { static_cast<float>(x4), static_cast<float>(y4) };
    verts[3].color = color;
    verts[3].tex_coord = {0, 0};

    int indices[6] = { 0, 1, 2, 0, 2, 3 };
    SDL_RenderGeometry(ren_, nullptr, verts, 4, indices, 6);
}

void Renderer::renderTriangle(float x1, float y1, float x2, float y2, float x3, float y3, SDL_Color c1, SDL_Color c2, SDL_Color c3) {
    SDL_Vertex verts[3];
    verts[0].position = {x1, y1};
    verts[0].color = c1;
    verts[0].tex_coord = {0, 0};
    verts[1].position = {x2, y2};
    verts[1].color = c2;
    verts[1].tex_coord = {0, 0};
    verts[2].position = {x3, y3};
    verts[2].color = c3;
    verts[2].tex_coord = {0, 0};

    int indices[3] = {0, 1, 2};
    SDL_RenderGeometry(ren_, nullptr, verts, 3, indices, 3);
}

void Renderer::renderThickLine(float x1, float y1, float x2, float y2, float thickness, SDL_Color color) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;

    float nx = (-dy / len) * (thickness * 0.5f);
    float ny = ( dx / len) * (thickness * 0.5f);

    SDL_Vertex verts[4];
    verts[0].position = {x1 + nx, y1 + ny};
    verts[0].color = color;
    verts[0].tex_coord = {0, 0};
    verts[1].position = {x2 + nx, y2 + ny};
    verts[1].color = color;
    verts[1].tex_coord = {0, 0};
    verts[2].position = {x2 - nx, y2 - ny};
    verts[2].color = color;
    verts[2].tex_coord = {0, 0};
    verts[3].position = {x1 - nx, y1 - ny};
    verts[3].color = color;
    verts[3].tex_coord = {0, 0};

    int indices[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(ren_, nullptr, verts, 4, indices, 6);
}

void Renderer::renderSky(float carX, float skyScrollX, float horizonY, float raceTime, int viewW, int viewH) {
    int hY = static_cast<int>(horizonY);

    // 1. Cielo con gradiente suave por hardware (Top -> Mid -> Horizon)
    SDL_Color skyTop = {15, 23, 42, 255};     // Azul noche profundo
    SDL_Color skyMid = {88, 28, 135, 255};    // Púrpura synth
    SDL_Color skyBot = {244, 63, 94, 255};    // Coral / Magenta neón en el horizonte

    int splitY = hY * 55 / 100;
    {
        SDL_Vertex vTop[4] = {
            {{0.0f, 0.0f}, skyTop, {0, 0}},
            {{static_cast<float>(viewW), 0.0f}, skyTop, {0, 0}},
            {{static_cast<float>(viewW), static_cast<float>(splitY)}, skyMid, {0, 0}},
            {{0.0f, static_cast<float>(splitY)}, skyMid, {0, 0}}
        };
        SDL_Vertex vBot[4] = {
            {{0.0f, static_cast<float>(splitY)}, skyMid, {0, 0}},
            {{static_cast<float>(viewW), static_cast<float>(splitY)}, skyMid, {0, 0}},
            {{static_cast<float>(viewW), static_cast<float>(hY + 20)}, skyBot, {0, 0}},
            {{0.0f, static_cast<float>(hY + 20)}, skyBot, {0, 0}}
        };
        int idx[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(ren_, nullptr, vTop, 4, idx, 6);
        SDL_RenderGeometry(ren_, nullptr, vBot, 4, idx, 6);
    }

    // 2. Sol Retro-Arcade Masivo con franjas horizontales animadas (Sin saltos bruscos)
    int sunX = (viewW / 2) - static_cast<int>(skyScrollX * 0.35f);
    sunX = ((sunX % 2400) + 2400) % 2400 - 240;
    int sunY = hY - 95;
    int sunR = 145;

    // Halo exterior cálido
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
    drawNeobrutalCircle(sunX, sunY, sunR + 28, {251, 113, 133, 65}, 0, 0);
    // Cuerpo del sol neobrutalista
    drawNeobrutalCircle(sunX, sunY, sunR, {253, 224, 71, 255}, 6, 8);

    // Recortes horizontales synthwave en la mitad inferior del sol
    for (int s = 0; s < 6; ++s) {
        int bandY = sunY + 15 + s * 20;
        int bandH = 4 + s * 2;
        if (bandY < hY) {
            int dy = bandY - sunY;
            int chordW = static_cast<int>(std::sqrt(std::max(0, sunR * sunR - dy * dy)));
            SDL_Rect cutR = {sunX - chordW + 4, bandY, (chordW - 4) * 2, bandH};
            SDL_SetRenderDrawColor(ren_, 244, 63, 94, 255);
            SDL_RenderFillRect(ren_, &cutR);
        }
    }

    // 3. Nubes estilizadas en movimiento lento
    for (int c = 0; c < 6; ++c) {
        float cloudBase = c * 420.0f + raceTime * (18.0f + c * 4.0f) - skyScrollX * 0.45f;
        int cx = static_cast<int>(std::fmod(cloudBase, 2500.0f));
        if (cx < 0) cx += 2500;
        cx -= 250;
        int cy = 110 + (c % 3) * 52;
        int cw = 150 + (c % 2) * 65;
        drawNeobrutalRect(cx, cy, cw, 26, {254, 240, 138, 220}, 3, 4);
        drawNeobrutalRect(cx + 24, cy - 16, cw - 48, 18, {255, 255, 255, 235}, 3, 0);
    }

    // 4. Bandada de pájaros animados en el cielo
    for (int b = 0; b < 5; ++b) {
        int bx = ((sunX + 260 + b * 48 + static_cast<int>(raceTime * 45.0f)) % (kScreenWidth + 100)) - 50;
        int by = 155 + (b % 3) * 22 + static_cast<int>(std::sin(raceTime * 6.0f + b) * 6.0f);
        int wingY = static_cast<int>(std::sin(raceTime * 11.0f + b * 1.4f) * 7.0f);
        renderThickLine(bx - 10, by + wingY, bx, by, 3.0f, {18, 18, 18, 255});
        renderThickLine(bx, by, bx + 10, by + wingY, 3.0f, {18, 18, 18, 255});
    }

    // 5. Doble Cordillera de Montañas con Parallax Suave y Sombras de Dos Caras
    // Capa lejana
    int farScroll = static_cast<int>(skyScrollX * 0.65f + carX * 45.0f);
    for (int i = -3; i < 14; ++i) {
        int mX = i * 240 - ((farScroll % 240 + 240) % 240);
        int seed = ((i + farScroll / 240) % 5 + 5) % 5;
        int mH = 135 + seed * 32;
        int topY = hY - mH;

        renderTriangle(mX, hY, mX + 140, topY, mX + 280, hY,
                       {49, 46, 129, 255}, {67, 56, 202, 255}, {30, 27, 75, 255});
        renderThickLine(mX, hY, mX + 140, topY, 4.0f, {18, 18, 18, 255});
        renderThickLine(mX + 140, topY, mX + 280, hY, 4.0f, {18, 18, 18, 255});
    }

    // Capa cercana con cara iluminada y cara en sombra + cumbres nevadas
    int nearScroll = static_cast<int>(skyScrollX * 1.15f + carX * 85.0f);
    for (int i = -3; i < 16; ++i) {
        int mX = i * 180 - ((nearScroll % 180 + 180) % 180);
        int seed = ((i + nearScroll / 180) % 4 + 4) % 4;
        int mH = 90 + seed * 36;
        int topY = hY - mH;
        int peakX = mX + 105;

        // Cara izquierda iluminada por el sol
        renderTriangle(mX, hY, peakX, topY, peakX, hY,
                       {99, 102, 241, 255}, {129, 140, 248, 255}, {79, 70, 229, 255});
        // Cara derecha en sombra profunda
        renderTriangle(peakX, hY, peakX, topY, mX + 210, hY,
                       {49, 46, 129, 255}, {55, 48, 163, 255}, {30, 27, 75, 255});

        // Cumbre nevada en los picos más altos
        if (mH >= 126) {
            float snowH = mH * 0.32f;
            float leftSnowX = peakX - (105.0f * 0.32f);
            float rightSnowX = peakX + (105.0f * 0.32f);
            renderTriangle(leftSnowX, topY + snowH, peakX, topY, peakX, topY + snowH,
                           {248, 250, 252, 255}, {255, 255, 255, 255}, {226, 232, 240, 255});
            renderTriangle(peakX, topY + snowH, peakX, topY, rightSnowX, topY + snowH,
                           {203, 213, 225, 255}, {226, 232, 240, 255}, {148, 163, 184, 255});
        }

        // Bordes negros neobrutalistas
        renderThickLine(mX, hY, peakX, topY, 4.5f, {18, 18, 18, 255});
        renderThickLine(peakX, topY, mX + 210, hY, 4.5f, {18, 18, 18, 255});
    }

    // Línea divisoria firme del horizonte
    renderThickLine(0, hY, viewW, hY, 4.0f, {18, 18, 18, 255});
}

void Renderer::renderRoad(const Road& road, const Car& car, float horizonY, float camTilt, float raceTime, int viewW, int viewH, const Car* otherCar, SDL_Color otherColor) {
    float playerZ = car.getTrackZ();
    float playerX = car.getX();

    int baseIdx = static_cast<int>(std::floor(playerZ / Road::kSegmentLength));
    float basePercent = (playerZ - baseIdx * Road::kSegmentLength) / Road::kSegmentLength;
    basePercent = std::clamp(basePercent, 0.0f, 1.0f);

    const RoadSegment& seg0 = road.getSegment(baseIdx);
    const RoadSegment& seg1 = road.getSegment(baseIdx + 1);

    // CORRECCIÓN DE BUG DE CÁMARA EN COLINAS:
    // La cámara ahora sigue suavemente la elevación (worldY) de la carretera bajo las ruedas del jugador
    float playerWorldY = seg0.worldY + (seg1.worldY - seg0.worldY) * basePercent;
    float cameraH = playerWorldY + kCameraHeight;

    float roadW = Road::kRoadWidth;

    struct ProjectedPoint {
        float screenX = 0.0f;
        float screenY = 0.0f;
        float screenW = 0.0f;
        float scale = 0.0f;
        float clipY = 1080.0f;
        float fog = 0.0f;
        float curve = 0.0f;
    };

    std::vector<ProjectedPoint> projected(kDrawDistance);

    // La cámara está montada dentro del auto y debe reflejar la posición exacta del piloto (playerX)
    float playerCamX = playerX;

    // INTERPOLACIÓN SUB-SEGMENTO HERMITE SUAVE:
    // Evita saltos bruscos cuando el auto pasa de un segmento al siguiente a alta velocidad
    float smoothPercent = basePercent * basePercent * (3.0f - 2.0f * basePercent);
    float interpolatedBaseCurve = seg0.curve + (seg1.curve - seg0.curve) * smoothPercent;

    float curX = 0.0f;
    const float curveScaleFactor = 280.0f; // Calibrado para fluidez y arco natural sin distorsión
    float ddx = -interpolatedBaseCurve * curveScaleFactor * smoothPercent;

    for (int n = 0; n < kDrawDistance; ++n) {
        const RoadSegment& seg = road.getSegment(baseIdx + n);
        const RoadSegment& nextSeg = road.getSegment(baseIdx + n + 1);

        // Curvatura suavemente interpolada entre segmentos adyacentes
        float segCurve = seg.curve + (nextSeg.curve - seg.curve) * smoothPercent;

        float relZ = (static_cast<float>(n) - basePercent) * Road::kSegmentLength;
        if (relZ < 1.5f) relZ = 1.5f;

        float scale = kCameraDepth / relZ;

        // Atenuación progresiva de campo cercano (Near-field damping):
        // En los primeros 6 segmentos (~70m delante del capó) la carretera permanece estable y no pega sacudidas laterales
        float nearDamping = std::clamp(static_cast<float>(n) / 6.0f, 0.0f, 1.0f);
        float effectiveCurX = curX * nearDamping;

        // La carretera se proyecta con respecto a la posición física del auto (playerCamX * roadW)
        float worldX = effectiveCurX - (playerCamX * roadW);
        float worldY = seg.worldY - cameraH;

        float sx = (viewW * 0.5f) + (scale * worldX);
        float sy = horizonY - (scale * worldY) + ((sx - viewW * 0.5f) * camTilt);
        float sw = scale * roadW;

        float distRatio = static_cast<float>(n) / static_cast<float>(kDrawDistance);
        // Niebla exponencial suave para disolver el horizonte sin cortes ni pop-in
        float fog = std::clamp(std::pow(distRatio, 1.6f), 0.0f, 1.0f);

        projected[n] = {sx, sy, sw, scale, static_cast<float>(viewH), fog, segCurve};

        curX += ddx;
        ddx += segCurve * curveScaleFactor;
    }

    // CORRECCIÓN DE BUG DE OCLUSIÓN EN COLINAS (CLIPY BUFFER):
    // Recorrido frontal (de adelante hacia atrás) para calcular la silueta más alta (menor Y en pantalla).
    // Cada segmento n almacena en clipY el punto más alto alcanzado por cualquier segmento más cercano (0 a n-1).
    float runningMinY = static_cast<float>(viewH);
    for (int n = 0; n < kDrawDistance; ++n) {
        projected[n].clipY = runningMinY;
        if (projected[n].screenY < runningMinY) {
            runningMinY = projected[n].screenY;
        }
    }

    SDL_Color fogColor = {244, 63, 94, 255}; // Bruma cálida del horizonte

    // 2. Renderizado de polígonos de pista (de atrás hacia adelante, respetando oclusión de crestas)
    for (int n = kDrawDistance - 1; n > 0; --n) {
        const auto& p1 = projected[n - 1]; // Más cerca
        const auto& p2 = projected[n];     // Más lejos

        // Si el segmento está completamente detrás de una colina más cercana o por encima del horizonte, se omite
        if (p2.screenY >= p1.clipY) continue;
        if (p1.screenY <= p2.screenY) continue;

        float y1 = std::min(p1.screenY, p1.clipY);
        float y2 = std::max(p2.screenY, horizonY - 1.0f);
        if (y1 <= y2) continue;

        const RoadSegment& seg = road.getSegment(baseIdx + n - 1);

        SDL_Color grassCol  = lerpColor(seg.grassColor, fogColor, p1.fog);
        SDL_Color rumbleCol = lerpColor(seg.rumbleColor, fogColor, p1.fog);
        SDL_Color roadCol   = lerpColor(seg.roadColor, fogColor, p1.fog * 0.85f);
        SDL_Color shoulCol  = lerpColor(scaleColor(seg.grassColor, 0.78f), fogColor, p1.fog);

        int iy1 = static_cast<int>(y1);
        int iy2 = static_cast<int>(y2);

        // A. Césped lateral completo
        renderPolygon(0, iy1, viewW, iy1, viewW, iy2, 0, iy2, grassCol);

        // Franja de tierra/banquina exterior para mayor detalle en la carretera
        float dirtW1 = p1.screenW * 1.32f;
        float dirtW2 = p2.screenW * 1.32f;
        renderPolygon(static_cast<int>(p1.screenX - dirtW1), iy1,
                      static_cast<int>(p1.screenX + dirtW1), iy1,
                      static_cast<int>(p2.screenX + dirtW2), iy2,
                      static_cast<int>(p2.screenX - dirtW2), iy2, shoulCol);

        // B. Arcén / Pianos (Rumble strips)
        float rumbleW1 = p1.screenW * 1.15f;
        float rumbleW2 = p2.screenW * 1.15f;
        renderPolygon(static_cast<int>(p1.screenX - rumbleW1), iy1,
                      static_cast<int>(p1.screenX + rumbleW1), iy1,
                      static_cast<int>(p2.screenX + rumbleW2), iy2,
                      static_cast<int>(p2.screenX - rumbleW2), iy2, rumbleCol);

        // Borde negro exterior de los pianos (respetando recorte vertical para que no flote en el aire)
        renderThickLine(p1.screenX - rumbleW1, static_cast<float>(iy1), p2.screenX - rumbleW2, static_cast<float>(iy2), 3.0f, {18, 18, 18, 255});
        renderThickLine(p1.screenX + rumbleW1, static_cast<float>(iy1), p2.screenX + rumbleW2, static_cast<float>(iy2), 3.0f, {18, 18, 18, 255});

        // C. Asfalto principal
        renderPolygon(static_cast<int>(p1.screenX - p1.screenW), iy1,
                      static_cast<int>(p1.screenX + p1.screenW), iy1,
                      static_cast<int>(p2.screenX + p2.screenW), iy2,
                      static_cast<int>(p2.screenX - p2.screenW), iy2, roadCol);

        // Huellas sutiles de neumáticos (racing groove) en los 3 carriles
        SDL_Color grooveCol = scaleColor(roadCol, 0.86f);
        const float laneCenters[3] = {-0.66f, 0.0f, 0.66f};
        for (float lc : laneCenters) {
            float gx1 = p1.screenX + lc * p1.screenW;
            float gx2 = p2.screenX + lc * p2.screenW;
            float gw1 = p1.screenW * 0.16f;
            float gw2 = p2.screenW * 0.16f;
            renderPolygon(static_cast<int>(gx1 - gw1), iy1,
                          static_cast<int>(gx1 + gw1), iy1,
                          static_cast<int>(gx2 + gw2), iy2,
                          static_cast<int>(gx2 - gw2), iy2, grooveCol);
        }

        // Líneas continuas amarillas de borde de pista
        SDL_Color edgeLineCol = lerpColor({250, 204, 21, 255}, fogColor, p1.fog);
        float elW1 = std::max(1.5f, p1.screenW * 0.016f);
        float elW2 = std::max(1.0f, p2.screenW * 0.016f);
        renderPolygon(static_cast<int>(p1.screenX - p1.screenW * 0.96f - elW1), iy1,
                      static_cast<int>(p1.screenX - p1.screenW * 0.96f + elW1), iy1,
                      static_cast<int>(p2.screenX - p2.screenW * 0.96f + elW2), iy2,
                      static_cast<int>(p2.screenX - p2.screenW * 0.96f - elW2), iy2, edgeLineCol);
        renderPolygon(static_cast<int>(p1.screenX + p1.screenW * 0.96f - elW1), iy1,
                      static_cast<int>(p1.screenX + p1.screenW * 0.96f + elW1), iy1,
                      static_cast<int>(p2.screenX + p2.screenW * 0.96f + elW2), iy2,
                      static_cast<int>(p2.screenX + p2.screenW * 0.96f - elW2), iy2, edgeLineCol);

        // Borde negro entre asfalto y pianos (respetando iy1 e iy2)
        renderThickLine(p1.screenX - p1.screenW, static_cast<float>(iy1), p2.screenX - p2.screenW, static_cast<float>(iy2), 2.5f, {18, 18, 18, 255});
        renderThickLine(p1.screenX + p1.screenW, static_cast<float>(iy1), p2.screenX + p2.screenW, static_cast<float>(iy2), 2.5f, {18, 18, 18, 255});

        // D. Líneas discontinuas de división: 3 CARRILES + Catadióptricos (Ojos de gato)
        if (seg.laneColor.r > 100) {
            SDL_Color laneCol = lerpColor(seg.laneColor, fogColor, p1.fog);
            float laneW1 = std::max(1.5f, p1.screenW * 0.022f);
            float laneW2 = std::max(1.0f, p2.screenW * 0.022f);

            float l1_x1 = p1.screenX - p1.screenW * 0.333f;
            float l1_x2 = p2.screenX - p2.screenW * 0.333f;
            renderPolygon(static_cast<int>(l1_x1 - laneW1), iy1,
                          static_cast<int>(l1_x1 + laneW1), iy1,
                          static_cast<int>(l1_x2 + laneW2), iy2,
                          static_cast<int>(l1_x2 - laneW2), iy2, laneCol);

            float l2_x1 = p1.screenX + p1.screenW * 0.333f;
            float l2_x2 = p2.screenX + p2.screenW * 0.333f;
            renderPolygon(static_cast<int>(l2_x1 - laneW1), iy1,
                          static_cast<int>(l2_x1 + laneW1), iy1,
                          static_cast<int>(l2_x2 + laneW2), iy2,
                          static_cast<int>(l2_x2 - laneW2), iy2, laneCol);
        } else if (seg.index % 3 == 0 && p1.screenW > 80.0f) {
            // Pequeños reflectores naranjas entre las líneas discontinuas
            int studW = std::max(3, static_cast<int>(p1.screenW * 0.015f));
            int studH = std::max(2, static_cast<int>(p1.screenW * 0.008f));
            drawNeobrutalRect(static_cast<int>(p1.screenX - p1.screenW * 0.333f) - studW / 2, iy1 - studH, studW, studH, {251, 146, 60, 255}, 1, 0);
            drawNeobrutalRect(static_cast<int>(p1.screenX + p1.screenW * 0.333f) - studW / 2, iy1 - studH, studW, studH, {251, 146, 60, 255}, 1, 0);
        }

        // E. Guardarraíles metálicos exteriores en curvas pronunciadas
        if (std::abs(seg.curve) > 2.2f && n < kDrawDistance - 8) {
            float sideSign = (seg.curve > 0.0f) ? -1.0f : 1.0f; // Lado exterior de la curva
            float gx1 = p1.screenX + sideSign * p1.screenW * 1.24f;
            float gx2 = p2.screenX + sideSign * p2.screenW * 1.24f;
            float gh1 = p1.screenW * 0.08f;
            float gh2 = p2.screenW * 0.08f;
            SDL_Color railCol = (seg.index % 2 == 0) ? SDL_Color{226, 232, 240, 255} : SDL_Color{148, 163, 184, 255};
            renderPolygon(static_cast<int>(gx1), static_cast<int>(y1 - gh1 * 0.4f),
                          static_cast<int>(gx2), static_cast<int>(y2 - gh2 * 0.4f),
                          static_cast<int>(gx2), static_cast<int>(y2 - gh2),
                          static_cast<int>(gx1), static_cast<int>(y1 - gh1), railCol);
            renderThickLine(gx1, y1 - gh1, gx2, y2 - gh2, 2.0f, {18, 18, 18, 255});
        }
    }

    // 3. RECOPILACIÓN DE PROPS Y TRÁFICO INTERCALADOS POR PROFUNDIDAD (PAINTER'S ALGORITHM REAL)
    // Antes los coches se dibujaban siempre encima de los árboles/arcos aunque estuvieran detrás.
    // Ahora se ordenan por cada segmento n desde el horizonte hacia el jugador y se interpolan suavemente.
    struct TrafficSliceItem {
        TrafficCar car;
        float interpX;
        float interpY;
        float interpW;
        float clipY;
        float curve;
    };
    std::vector<std::vector<TrafficSliceItem>> sliceTraffic(kDrawDistance);

    float trackLen = road.getTrackLength();

    // Inyectar el coche del otro jugador en el buffer de profundidad si está delante y dentro de distancia visual
    if (otherCar) {
        float relZ = otherCar->getTrackZ() - playerZ;
        while (relZ > trackLen * 0.5f) relZ -= trackLen;
        while (relZ < -trackLen * 0.5f) relZ += trackLen;

        if (relZ > 2.0f && relZ < (kDrawDistance - 2) * Road::kSegmentLength) {
            float exactIdx = (relZ / Road::kSegmentLength) + basePercent;
            int idx0 = static_cast<int>(std::floor(exactIdx));
            float frac = exactIdx - idx0;

            if (idx0 >= 0 && idx0 + 1 < kDrawDistance) {
                const auto& p0 = projected[idx0];
                const auto& p1 = projected[idx0 + 1];

                float ix = p0.screenX + (p1.screenX - p0.screenX) * frac;
                float iy = p0.screenY + (p1.screenY - p0.screenY) * frac;
                float iw = p0.screenW + (p1.screenW - p0.screenW) * frac;

                int cX = static_cast<int>(ix + otherCar->getX() * iw);
                TrafficCar rivalCar;
                rivalCar.x = otherCar->getX();
                rivalCar.z = otherCar->getTrackZ();
                rivalCar.speed = otherCar->getSpeed();
                rivalCar.bodyColor = otherColor;

                sliceTraffic[idx0].push_back({rivalCar, static_cast<float>(cX), iy, iw, p0.clipY, p0.curve});
            }
        }
    }

    for (const auto& tCar : road.getTraffic()) {
        float relZ = tCar.z - playerZ;
        while (relZ > trackLen * 0.5f) relZ -= trackLen;
        while (relZ < -trackLen * 0.5f) relZ += trackLen;

        if (relZ > 2.0f && relZ < (kDrawDistance - 2) * Road::kSegmentLength) {
            // Posición exacta en el buffer proyectado (con fracción para interpolar sin saltos)
            float exactIdx = (relZ / Road::kSegmentLength) + basePercent;
            int idx0 = static_cast<int>(std::floor(exactIdx));
            float frac = exactIdx - idx0;

            if (idx0 >= 0 && idx0 + 1 < kDrawDistance) {
                const auto& p0 = projected[idx0];
                const auto& p1 = projected[idx0 + 1];

                float ix = p0.screenX + (p1.screenX - p0.screenX) * frac;
                float iy = p0.screenY + (p1.screenY - p0.screenY) * frac;
                float iw = p0.screenW + (p1.screenW - p0.screenW) * frac;

                int carX = static_cast<int>(ix + tCar.x * iw);
                sliceTraffic[idx0].push_back({tCar, static_cast<float>(carX), iy, iw, p0.clipY, p0.curve});
            }
        }
    }

    // 4. Renderizar Props y Coches desde el segmento más lejano al más cercano
    for (int n = kDrawDistance - 2; n >= 1; --n) {
        const auto& p = projected[n];
        const RoadSegment& seg = road.getSegment(baseIdx + n);

        // A. Props del segmento (solo si su base no está oculta tras una colina cercana)
        if (p.screenY <= p.clipY + 4.0f && p.screenY > horizonY) {
            for (const auto& prop : seg.props) {
                int propX = static_cast<int>(p.screenX + prop.offset * p.screenW);
                renderProp(prop, propX, static_cast<int>(p.screenY), p.screenW, p.clipY, raceTime, seg.index, viewW);
            }
        }

        // B. Coches de tráfico ubicados en este segmento
        for (const auto& item : sliceTraffic[n]) {
            if (item.interpY <= item.clipY + 12.0f && item.interpY > horizonY) {
                renderTrafficCar(item.car, static_cast<int>(item.interpX), static_cast<int>(item.interpY),
                                 item.interpW, item.clipY, item.curve, raceTime);
            }
        }
    }
}

void Renderer::renderProp(const RoadProp& prop, int screenX, int screenY, float roadScreenW, float clipY, float raceTime, int segIndex, int viewW) {
    // Eliminado el corte artificial "screenY > 690" que hacía desaparecer los árboles antes de llegar al coche
    if (roadScreenW <= 4.0f || screenY > clipY + 8.0f || screenX < -500 || screenX > viewW + 500) return;

    // Oscilación orgánica de viento para que los árboles tengan vida
    float windSway = std::sin(raceTime * 3.2f + segIndex * 0.7f) * (roadScreenW * 0.025f);

    if (prop.type == PropType::PALM_TREE) {
        int trunkW = std::clamp(static_cast<int>(roadScreenW * 0.065f), 2, 58);
        int trunkH = std::clamp(static_cast<int>(roadScreenW * 0.92f), 8, 540);
        int curveDir = (prop.offset < 0.0f) ? 1 : -1; // Se inclinan hacia la pista

        // Sombra proyectada en la base sobre el césped
        int shadowW = trunkW * 4;
        int shadowH = std::max(2, trunkW / 2);
        drawNeobrutalRect(screenX - shadowW / 2, screenY - shadowH / 2, shadowW, shadowH, {15, 60, 28, 255}, 1, 0);

        // Tronco segmentado en 5 anillos curvados que se mecen con el viento
        int topX = screenX;
        int topY = screenY - trunkH;
        const int rings = 5;
        for (int r = 0; r < rings; ++r) {
            float t0 = static_cast<float>(r) / rings;
            float t1 = static_cast<float>(r + 1) / rings;
            int y0 = screenY - static_cast<int>(t0 * trunkH);
            int y1 = screenY - static_cast<int>(t1 * trunkH);
            int x0 = screenX + static_cast<int>((t0 * t0) * (trunkW * 1.4f * curveDir + windSway));
            int x1 = screenX + static_cast<int>((t1 * t1) * (trunkW * 1.4f * curveDir + windSway));
            int w0 = std::max(3, static_cast<int>(trunkW * (1.0f - t0 * 0.32f)));
            int w1 = std::max(2, static_cast<int>(trunkW * (1.0f - t1 * 0.32f)));

            SDL_Color barkCol = (r % 2 == 0) ? SDL_Color{180, 83, 9, 255} : SDL_Color{146, 64, 14, 255};
            renderPolygon(x0 - w0 / 2, y0, x0 + w0 / 2, y0, x1 + w1 / 2, y1, x1 - w1 / 2, y1, barkCol);
            renderThickLine(x0 - w0 / 2, y0, x1 - w1 / 2, y1, 2.5f, {18, 18, 18, 255});
            renderThickLine(x0 + w0 / 2, y0, x1 + w1 / 2, y1, 2.5f, {18, 18, 18, 255});
            if (r == rings - 1) {
                topX = x1;
                topY = y1;
            }
        }

        // Cocos en el centro de la copa
        int cocoR = std::clamp(trunkW / 3, 2, 14);
        drawNeobrutalCircle(topX - cocoR, topY + cocoR, cocoR, {92, 45, 12, 255}, 2, 0);
        drawNeobrutalCircle(topX + cocoR, topY + cocoR, cocoR, {92, 45, 12, 255}, 2, 0);

        // Hojas de palmera arqueadas dinámicas (6 penachos tropicales con bordes neobrutalistas)
        int frondLen = std::clamp(static_cast<int>(roadScreenW * 0.42f), 12, 250);
        const float angles[6] = {-2.65f, -2.05f, -1.45f, -0.45f, -1.05f, -0.15f};
        for (int f = 0; f < 6; ++f) {
            float ang = angles[f] + (windSway * 0.004f);
            int midX = topX + static_cast<int>(std::cos(ang) * frondLen * 0.55f);
            int midY = topY + static_cast<int>(std::sin(ang) * frondLen * 0.45f) - frondLen / 5;
            int tipX = topX + static_cast<int>(std::cos(ang) * frondLen);
            int tipY = topY + static_cast<int>(std::sin(ang) * frondLen * 0.35f) + frondLen / 4;

            SDL_Color leafCol = (f % 2 == 0) ? SDL_Color{22, 163, 74, 255} : SDL_Color{74, 222, 128, 255};
            renderTriangle(topX, topY, midX, midY, tipX, tipY, leafCol, leafCol, scaleColor(leafCol, 0.85f));
            renderThickLine(topX, topY, midX, midY, 2.5f, {18, 18, 18, 255});
            renderThickLine(midX, midY, tipX, tipY, 2.5f, {18, 18, 18, 255});
            renderThickLine(tipX, tipY, topX, topY, 2.0f, {18, 18, 18, 255});
        }

    } else if (prop.type == PropType::PINE_TREE) {
        int trunkW = std::clamp(static_cast<int>(roadScreenW * 0.06f), 2, 46);
        int trunkH = std::clamp(static_cast<int>(roadScreenW * 0.90f), 8, 480);

        // Sombra de base
        int shadowW = trunkW * 4;
        drawNeobrutalRect(screenX - shadowW / 2, screenY - 3, shadowW, std::max(2, trunkW / 2), {15, 60, 28, 255}, 1, 0);

        // Tronco de madera con corteza iluminada
        int baseH = trunkH / 3;
        drawNeobrutalRect(screenX - trunkW / 2, screenY - baseH, trunkW, baseH, {120, 53, 15, 255}, 1, 1);
        drawNeobrutalRect(screenX - trunkW / 2 + 1, screenY - baseH + 1, std::max(1, trunkW / 3), std::max(1, baseH - 2), {180, 83, 9, 255}, 0, 0);

        // 3 Niveles de follaje piramidal con iluminación izquierda y sombra derecha
        int maxCrownW = std::clamp(static_cast<int>(roadScreenW * 0.44f), 6, 280);
        for (int layer = 0; layer < 3; ++layer) {
            float layerScale = 1.0f - layer * 0.23f;
            int lw = static_cast<int>(maxCrownW * layerScale);
            int lh = static_cast<int>(trunkH * 0.38f * layerScale);
            int bottomY = screenY - baseH - static_cast<int>(layer * trunkH * 0.21f);
            int apexY = bottomY - lh;
            int apexX = screenX + static_cast<int>(windSway * (0.4f + layer * 0.3f));

            SDL_Color lightGreen = {34, 197, 94, 255};
            SDL_Color darkGreen  = {21, 128, 61, 255};

            // Mitad izquierda iluminada
            renderTriangle(screenX - lw / 2, bottomY, apexX, apexY, screenX, bottomY,
                           lightGreen, {74, 222, 128, 255}, lightGreen);
            // Mitad derecha sombreada
            renderTriangle(screenX, bottomY, apexX, apexY, screenX + lw / 2, bottomY,
                           darkGreen, lightGreen, {20, 83, 45, 255});

            // Contorno neobrutalista
            renderThickLine(screenX - lw / 2, bottomY, apexX, apexY, 3.0f, {18, 18, 18, 255});
            renderThickLine(apexX, apexY, screenX + lw / 2, bottomY, 3.0f, {18, 18, 18, 255});
            renderThickLine(screenX - lw / 2, bottomY, screenX + lw / 2, bottomY, 3.0f, {18, 18, 18, 255});
        }

    } else if (prop.type == PropType::LAMPPOST) {
        // IMPLEMENTACIÓN DE FAROLAS LATERALES (Antes faltaban en renderProp)
        int poleW = std::clamp(static_cast<int>(roadScreenW * 0.022f), 2, 18);
        int poleH = std::clamp(static_cast<int>(roadScreenW * 0.88f), 20, 480);
        int armDir = (prop.offset < 0.0f) ? 1 : -1; // El brazo apunta hacia el centro de la carretera
        int armLen = std::clamp(static_cast<int>(roadScreenW * 0.22f), 6, 130);

        // Poste vertical metálico
        drawNeobrutalRect(screenX - poleW / 2, screenY - poleH, poleW, poleH, {71, 85, 105, 255}, 2, 2);
        // Brazo horizontal superior
        int armX = (armDir > 0) ? screenX : (screenX - armLen);
        drawNeobrutalRect(armX, screenY - poleH, armLen, std::max(3, poleW), {100, 116, 139, 255}, 2, 0);

        // Luminaria LED amarilla brillante con halo de luz
        int lampX = screenX + armDir * armLen;
        int lampY = screenY - poleH + poleW;
        int lampR = std::clamp(poleW * 2, 4, 28);
        SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
        drawNeobrutalCircle(lampX, lampY, lampR * 2, {253, 224, 71, 65}, 0, 0);
        drawNeobrutalCircle(lampX, lampY, lampR, {254, 240, 138, 255}, 2, 0);

    } else if (prop.type == PropType::ROCK) {
        // IMPLEMENTACIÓN DE ROCAS FACETEADAS (Antes faltaban en renderProp)
        int rW = std::clamp(static_cast<int>(roadScreenW * 0.22f), 8, 150);
        int rH = std::clamp(static_cast<int>(roadScreenW * 0.14f), 6, 95);

        // Polígono rocoso con 2 facetas de luz
        renderTriangle(screenX - rW / 2, screenY, screenX - rW / 6, screenY - rH, screenX + rW / 6, screenY,
                       {148, 163, 184, 255}, {203, 213, 225, 255}, {100, 116, 139, 255});
        renderTriangle(screenX + rW / 6, screenY, screenX - rW / 6, screenY - rH, screenX + rW / 2, screenY,
                       {71, 85, 105, 255}, {100, 116, 139, 255}, {51, 65, 85, 255});
        renderThickLine(screenX - rW / 2, screenY, screenX - rW / 6, screenY - rH, 3.0f, {18, 18, 18, 255});
        renderThickLine(screenX - rW / 6, screenY - rH, screenX + rW / 2, screenY, 3.0f, {18, 18, 18, 255});
        renderThickLine(screenX - rW / 2, screenY, screenX + rW / 2, screenY, 3.0f, {18, 18, 18, 255});

    } else if (prop.type == PropType::BILLBOARD_NEO) {
        int bW = std::clamp(static_cast<int>(roadScreenW * 0.50f), 26, 380);
        int bH = std::clamp(static_cast<int>(roadScreenW * 0.25f), 16, 190);
        int postH = std::clamp(static_cast<int>(roadScreenW * 0.22f), 12, 165);

        // Vigas de soporte
        int postW = std::max(3, bW / 22);
        drawNeobrutalRect(screenX - bW / 3, screenY - postH, postW, postH, {75, 85, 99, 255}, 2, 2);
        drawNeobrutalRect(screenX + bW / 3 - postW, screenY - postH, postW, postH, {75, 85, 99, 255}, 2, 2);

        // Marco exterior del cartel con estética arcade variada según segmento
        SDL_Color boardColor = (segIndex % 48 == 0) ? SDL_Color{250, 204, 21, 255} : SDL_Color{56, 189, 248, 255};
        SDL_Color accentColor = (segIndex % 48 == 0) ? SDL_Color{244, 63, 94, 255} : SDL_Color{250, 204, 21, 255};

        int topY = screenY - postH - bH;
        drawNeobrutalRect(screenX - bW / 2, topY, bW, bH, boardColor, 4, 5);
        // Franja decorativa diagonal/lateral
        drawNeobrutalRect(screenX - bW / 2 + 6, topY + 6, bW / 4, bH - 12, accentColor, 2, 0);

        if (bW > 85) {
            const char* label = (segIndex % 48 == 0) ? "TURBO" : "APEX";
            drawText(fontSmall_, label, screenX + bW / 10, topY + bH / 2 - 12, {18, 18, 18, 255}, true);
        }

    } else if (prop.type == PropType::CHECKPOINT_ARCH) {
        int archW = std::clamp(static_cast<int>(roadScreenW * 2.10f), 60, 1750);
        int archH = std::clamp(static_cast<int>(roadScreenW * 0.22f), 16, 145);
        int postH = std::clamp(static_cast<int>(roadScreenW * 0.72f), 28, 420);
        int pillarW = std::max(6, archW / 36);

        // Pilares laterales con franjas de competición
        drawNeobrutalRect(screenX - archW / 2, screenY - postH, pillarW, postH, {243, 244, 246, 255}, 3, 4);
        drawNeobrutalRect(screenX + archW / 2 - pillarW, screenY - postH, pillarW, postH, {243, 244, 246, 255}, 3, 4);

        // Pancarta superior principal con damero de carrera (Checkered Flag)
        int bannerY = screenY - postH - archH;
        drawNeobrutalRect(screenX - archW / 2 - 8, bannerY, archW + 16, archH, {244, 63, 94, 255}, 4, 6);

        // Damero blanco y negro en los extremos del arco
        int checkW = std::max(6, archW / 28);
        int checkH = std::max(6, archH / 2);
        for (int r = 0; r < 2; ++r) {
            for (int c = 0; c < 3; ++c) {
                SDL_Color sqCol = ((r + c) % 2 == 0) ? SDL_Color{255, 255, 255, 255} : SDL_Color{18, 18, 18, 255};
                drawNeobrutalRect(screenX - archW / 2 + c * checkW, bannerY + r * checkH, checkW, checkH, sqCol, 0, 0);
                drawNeobrutalRect(screenX + archW / 2 - (c + 1) * checkW, bannerY + r * checkH, checkW, checkH, sqCol, 0, 0);
            }
        }

        if (archW > 200) {
            drawText(fontHud_, "CHECKPOINT", screenX, bannerY + archH / 2 - 18, {255, 255, 255, 255}, true);
        }

    } else if (prop.type == PropType::FINISH_LINE_ARCH) {
        int archW = std::clamp(static_cast<int>(roadScreenW * 2.15f), 60, 1800);
        int archH = std::clamp(static_cast<int>(roadScreenW * 0.25f), 18, 160);
        int postH = std::clamp(static_cast<int>(roadScreenW * 0.72f), 28, 420);
        int pillarW = std::max(6, archW / 32);

        // Pilares dorados/blancos
        drawNeobrutalRect(screenX - archW / 2, screenY - postH, pillarW, postH, {250, 204, 21, 255}, 3, 4);
        drawNeobrutalRect(screenX + archW / 2 - pillarW, screenY - postH, pillarW, postH, {250, 204, 21, 255}, 3, 4);

        // Banner de META / FINISH con damero completo
        int bannerY = screenY - postH - archH;
        drawNeobrutalRect(screenX - archW / 2 - 8, bannerY, archW + 16, archH, {18, 18, 18, 255}, 4, 6);

        // Damero blanco y negro en todo el ancho superior del arco de meta
        int checkW = std::max(8, archW / 18);
        int checkH = std::max(6, archH / 3);
        int cols = (archW + 16) / checkW + 1;
        for (int r = 0; r < 2; ++r) {
            for (int c = 0; c < cols; ++c) {
                if ((r + c) % 2 == 0) {
                    drawNeobrutalRect(screenX - archW / 2 - 8 + c * checkW, bannerY + r * checkH, checkW, checkH, {255, 255, 255, 255}, 0, 0);
                }
            }
        }

        if (archW > 200) {
            drawText(fontTitle_, "START / FINISH", screenX, bannerY + archH / 2 - 14, {250, 204, 21, 255}, true);
        }
    }
}

void Renderer::renderTrafficCar(const TrafficCar& car, int screenX, int screenY, float roadScreenW, float clipY, float segCurve, float raceTime) {
    // Eliminado el corte "screenY > 690" que hacía desaparecer a los autos cuando estaban cerca del jugador
    if (roadScreenW <= 4.0f || screenY > clipY + 15.0f || screenX < -400 || screenX > kScreenWidth + 400) return;

    // Dimensiones 3D equilibradas: con kRoadWidth = 4400 (cada carril mide ~1466 unidades 3D),
    // el coche con ancho 2100 ocupa ~48% del carril, viéndose imponente pero con amplio espacio para maniobrar y rebasar.
    float w = 2100.0f; // Ancho 3D del coche rival (proporcional al carril ampliado)
    float h = 1040.0f; // Alto 3D del coche rival
    float l = 2700.0f; // Largo 3D del coche rival

    float scale = roadScreenW / Road::kRoadWidth;
    if (scale <= 0.0001f) return;

    // Escala continua y realista sin mínimos artificiales que deformen la perspectiva
    if (w * scale < 6.0f) {
        scale = 6.0f / w;
    }

    // Vibración de suspensión del coche de tráfico + inclinación en curvas
    float bounceY = std::sin(raceTime * 14.0f + car.z * 0.15f) * (2.0f * scale * 2.0f);
    int baseScreenY = screenY + static_cast<int>(bounceY);

    // Orientación 3D combinando posición transversal y curvatura de la pista
    float yaw = -car.x * 0.10f + segCurve * 0.030f;
    float cosY = std::cos(yaw);
    float sinY = std::sin(yaw);

    // 1. Sombra proyectada en el asfalto bajo el vehículo
    int shadowW = static_cast<int>(w * scale * 1.05f);
    int shadowH = std::max(4, static_cast<int>(h * scale * 0.20f));
    drawNeobrutalRect(screenX - shadowW / 2, screenY - shadowH / 2, shadowW, shadowH, {12, 12, 16, 220}, 0, 0);

    // 2. RUEDAS TRASERAS Y DELANTERAS CON NEUMÁTICOS ANCHOS Y LLANTAS
    int wheelW = std::max(4, static_cast<int>(w * scale * 0.18f));
    int wheelH = std::max(5, static_cast<int>(h * scale * 0.40f));
    int rearTrackHalf = static_cast<int>(w * scale * 0.46f);
    drawNeobrutalRect(screenX - rearTrackHalf - wheelW / 2, screenY - wheelH, wheelW, wheelH, {18, 18, 18, 255}, 2, 0);
    drawNeobrutalRect(screenX + rearTrackHalf - wheelW / 2, screenY - wheelH, wheelW, wheelH, {18, 18, 18, 255}, 2, 0);

    // Vértices 3D del modelo Low-Poly Deportivo
    struct Vec3 { float x, y, z; };
    std::vector<Vec3> localVerts = {
        // --- Chasis Inferior (0 a 7) ---
        {-w * 0.48f, h * 0.12f,  l * 0.50f},  // 0: Trasero izq abajo
        { w * 0.48f, h * 0.12f,  l * 0.50f},  // 1: Trasero der abajo
        { w * 0.45f, h * 0.12f, -l * 0.50f},  // 2: Frontal der abajo
        {-w * 0.45f, h * 0.12f, -l * 0.50f},  // 3: Frontal izq abajo
        {-w * 0.50f, h * 0.52f,  l * 0.50f},  // 4: Trasero izq cintura
        { w * 0.50f, h * 0.52f,  l * 0.50f},  // 5: Trasero der cintura
        { w * 0.48f, h * 0.44f, -l * 0.48f},  // 6: Frontal der cintura
        {-w * 0.48f, h * 0.44f, -l * 0.48f},  // 7: Frontal izq cintura

        // --- Cabina y Techo (8 a 11) ---
        {-w * 0.37f, h * 0.96f,  l * 0.16f},  // 8: Techo trasero izq
        { w * 0.37f, h * 0.96f,  l * 0.16f},  // 9: Techo trasero der
        { w * 0.34f, h * 0.90f, -l * 0.20f},  // 10: Techo delantero der
        {-w * 0.34f, h * 0.90f, -l * 0.20f},  // 11: Techo delantero izq

        // --- Alerón Deportivo Trasero (12 a 15) ---
        {-w * 0.47f, h * 0.72f,  l * 0.49f},  // 12: Alerón izq
        { w * 0.47f, h * 0.72f,  l * 0.49f},  // 13: Alerón der
        { w * 0.47f, h * 0.77f,  l * 0.38f},  // 14: Alerón der borde
        {-w * 0.47f, h * 0.77f,  l * 0.38f}   // 15: Alerón izq borde
    };

    struct Point2D { int x, y; float rotZ; };
    std::vector<Point2D> projected2D(localVerts.size());

    for (size_t i = 0; i < localVerts.size(); ++i) {
        float rx = localVerts[i].x * cosY + localVerts[i].z * sinY;
        float rz = -localVerts[i].x * sinY + localVerts[i].z * cosY;
        float ry = localVerts[i].y;

        int px = screenX + static_cast<int>(rx * scale);
        int py = baseScreenY - static_cast<int>(ry * scale);
        projected2D[i] = {px, py, rz};
    }

    // CORRECCIÓN DE BUG DE PROFUNDIDAD 3D:
    // Ahora depthZ se calcula usando la coordenada Z ROTADA (projected2D[v].rotZ),
    // evitando que las puertas laterales se dibujen encima de la trasera cuando el auto gira.
    struct MeshFace {
        int v[4];
        int count;
        float depthZ;
        SDL_Color color;
        bool hasBorder;
    };

    SDL_Color baseCol   = car.bodyColor;
    SDL_Color brightCol = lerpColor(baseCol, {255, 255, 255, 255}, 0.22f);
    SDL_Color shadowCol = scaleColor(baseCol, 0.75f);
    SDL_Color darkCol   = scaleColor(baseCol, 0.52f);
    SDL_Color glassCol  = {30, 41, 59, 255};

    auto calcFaceZ = [&](int i0, int i1, int i2, int i3) {
        return (projected2D[i0].rotZ + projected2D[i1].rotZ + projected2D[i2].rotZ + projected2D[i3].rotZ) * 0.25f;
    };

    std::vector<MeshFace> faces = {
        {{7, 6, 10, 11}, 4, calcFaceZ(7, 6, 10, 11), shadowCol, true}, // Parabrisas/Capó
        {{0, 3, 7, 4},   4, calcFaceZ(0, 3, 7, 4),   shadowCol, true}, // Lateral Izquierdo
        {{4, 7, 11, 8},  4, calcFaceZ(4, 7, 11, 8),  glassCol,  true}, // Ventanilla Izquierda
        {{1, 2, 6, 5},   4, calcFaceZ(1, 2, 6, 5),   darkCol,   true}, // Lateral Derecho
        {{5, 6, 10, 9},  4, calcFaceZ(5, 6, 10, 9),  glassCol,  true}, // Ventanilla Derecha
        {{8, 9, 10, 11}, 4, calcFaceZ(8, 9, 10, 11), brightCol, true}, // Techo iluminado
        {{4, 5, 9, 8},   4, calcFaceZ(4, 5, 9, 8),   glassCol,  true}, // Luneta Trasera
        {{0, 1, 5, 4},   4, calcFaceZ(0, 1, 5, 4) + 40.0f, baseCol, true}, // Panel Trasero (Prioridad visual)
        {{12, 13, 14, 15}, 4, calcFaceZ(12, 13, 14, 15) + 60.0f, darkCol, true} // Alerón Trasero
    };

    std::sort(faces.begin(), faces.end(), [](const MeshFace& a, const MeshFace& b) {
        return a.depthZ < b.depthZ;
    });

    for (const auto& f : faces) {
        renderPolygon(
            projected2D[f.v[0]].x, projected2D[f.v[0]].y,
            projected2D[f.v[1]].x, projected2D[f.v[1]].y,
            projected2D[f.v[2]].x, projected2D[f.v[2]].y,
            projected2D[f.v[3]].x, projected2D[f.v[3]].y,
            f.color
        );

        if (f.hasBorder) {
            for (int e = 0; e < f.count; ++e) {
                int next = (e + 1) % f.count;
                renderThickLine(
                    projected2D[f.v[e]].x, projected2D[f.v[e]].y,
                    projected2D[f.v[next]].x, projected2D[f.v[next]].y,
                    2.2f, {18, 18, 18, 255}
                );
            }
        }
    }

    // Reflejo diagonal en la luneta trasera
    int w8x = projected2D[8].x, w8y = projected2D[8].y;
    int w5x = projected2D[5].x, w5y = projected2D[5].y;
    if (std::abs(w5x - w8x) > 16) {
        renderThickLine(w8x + 6, w8y + 4, (w8x + w5x) / 2, w5y - 4, 2.5f, {148, 163, 184, 255});
    }

    // Soportes verticales del alerón
    renderThickLine(projected2D[4].x + 6, projected2D[4].y, projected2D[12].x + 6, projected2D[12].y, 3.0f, {18, 18, 18, 255});
    renderThickLine(projected2D[5].x - 6, projected2D[5].y, projected2D[13].x - 6, projected2D[13].y, 3.0f, {18, 18, 18, 255});

    // Detalles del Panel Trasero: Faros dobles con glow, difusor, matrícula y doble escape con llamarada
    int p0x = projected2D[0].x, p0y = projected2D[0].y;
    int p1x = projected2D[1].x;
    int p4y = projected2D[4].y;
    int p5y = projected2D[5].y;

    int rearW = p1x - p0x;
    int rearH = p0y - p4y;
    if (rearW > 16 && rearH > 8) {
        int lW = std::max(4, rearW / 5);
        int lH = std::max(3, rearH / 3);

        // Faros traseros LED rojos con centro naranja
        drawNeobrutalRect(p0x + 4, p4y + 4, lW, lH, {239, 68, 68, 255}, 2, 0);
        drawNeobrutalRect(p1x - lW - 4, p5y + 4, lW, lH, {239, 68, 68, 255}, 2, 0);
        if (lW > 8) {
            drawNeobrutalRect(p0x + 6, p4y + 6, lW / 2, std::max(2, lH - 4), {253, 224, 71, 255}, 0, 0);
            drawNeobrutalRect(p1x - lW / 2 - 6, p5y + 6, lW / 2, std::max(2, lH - 4), {253, 224, 71, 255}, 0, 0);
        }

        // Difusor inferior deportivo negro y escapes dobles
        int diffH = std::max(3, rearH / 4);
        drawNeobrutalRect(p0x + rearW / 6, p0y - diffH, rearW * 2 / 3, diffH, {24, 24, 27, 255}, 1, 0);

        // Llamarada / petardeo de escape animado
        bool flameOn = (static_cast<int>(raceTime * 16.0f + car.z) % 5 == 0);
        SDL_Color exCol = flameOn ? SDL_Color{251, 146, 60, 255} : SDL_Color{156, 163, 175, 255};
        int exR = std::max(2, diffH / 2);
        drawNeobrutalCircle(p0x + rearW / 4, p0y - diffH / 2, exR, exCol, 1, 0);
        drawNeobrutalCircle(p1x - rearW / 4, p0y - diffH / 2, exR, exCol, 1, 0);

        // Matrícula blanca
        int matW = std::max(6, rearW / 4);
        int matH = std::max(3, lH * 3 / 4);
        drawNeobrutalRect((p0x + p1x) / 2 - matW / 2, p4y + rearH / 3, matW, matH, {254, 249, 195, 255}, 1, 0);
    }
}

void Renderer::renderSpeedEffects(const Car& car, float horizonY, float raceTime, int viewW, int viewH) {
    float speedRatio = car.getSpeedRatio();
    if (speedRatio < 0.62f && !car.isDrifting()) return;

    // Líneas de velocidad estilo Anime / Initial D alrededor del parabrisas para máxima sensación de velocidad
    float intensity = std::clamp((speedRatio - 0.62f) / 0.38f, 0.0f, 1.0f);
    if (car.isDrifting()) intensity = std::max(intensity, 0.65f);

    int numLines = static_cast<int>(10 + intensity * 18);
    float centerX = viewW * 0.5f;
    float centerY = horizonY - 20.0f;

    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
    Uint8 alpha = static_cast<Uint8>(80 + intensity * 145);
    SDL_Color streakCol = {255, 255, 255, alpha};

    int timeTick = static_cast<int>(raceTime * 28.0f);
    for (int i = 0; i < numLines; ++i) {
        float angle = ((i * 137 + timeTick * 29) % 360) * (kPi / 180.0f);
        // Evitar la zona baja ocupada por el salpicadero
        if (std::sin(angle) > 0.38f) continue;

        float innerR = (viewH * 0.42f) + ((i * 73 + timeTick * 41) % 180);
        float outerR = innerR + 100.0f + intensity * 160.0f;

        float x1 = centerX + std::cos(angle) * innerR * 1.35f;
        float y1 = centerY + std::sin(angle) * innerR * 0.75f;
        float x2 = centerX + std::cos(angle) * outerR * 1.35f;
        float y2 = centerY + std::sin(angle) * outerR * 0.75f;

        renderThickLine(x1, y1, x2, y2, 2.5f + intensity * 2.0f, streakCol);
    }
}

void Renderer::renderCockpit(const Car& car, const Road& road, float raceTime, int viewW, int viewH, bool isSplitScreen, const Car* otherCar, SDL_Color otherColor) {
    if (isSplitScreen) {
        // =========================================================================
        // CABINA EN PANTALLA DIVIDIDA (1920x540 POR JUGADOR)
        // VISTA ULTRA-WIDE DEPORTIVA, MÁXIMA VISIBILIDAD DE PISTA Y ERGONOMÍA REALISTA
        // =========================================================================
        float speedRatio = car.getSpeedRatio();
        float bobAmp = (1.0f + speedRatio * 3.0f) + (car.isOffRoad() ? 5.0f : 0.0f);

        // Desplazamiento orgánico del chasis por fuerzas G y suspensión
        int swayX = static_cast<int>(-car.getRoll() * 3.5f + std::cos(car.getSuspensionBob() * 0.5f) * (bobAmp * 0.5f));
        int swayY = static_cast<int>( car.getPitch() * 1.4f + std::sin(car.getSuspensionBob()) * bobAmp);

        // 1. CAPÓ DELANTERO Y LIMPIAPARABRISAS
        int dashY = 355 + swayY;
        int hoodY = dashY - 6;

        // Capó rojo deportivo visible en la parte inferior del cristal
        renderPolygon(320 + swayX, hoodY + 8, 620 + swayX, hoodY + 8,
                      590 + swayX, hoodY + 30, 280 + swayX, hoodY + 30, {190, 45, 35, 255});
        renderThickLine(320 + swayX, hoodY + 8, 620 + swayX, hoodY + 8, 3.0f, {18, 18, 18, 255});

        // Limpiaparabrisas dobles negros en reposo
        renderThickLine(560 + swayX, hoodY - 2, 920 + swayX, hoodY - 14, 3.5f, {24, 26, 30, 255});
        renderThickLine(970 + swayX, hoodY - 2, 1420 + swayX, hoodY - 13, 3.5f, {24, 26, 30, 255});
        drawNeobrutalCircle(560 + swayX, hoodY - 2, 4, {18, 18, 18, 255}, 1, 0);
        drawNeobrutalCircle(970 + swayX, hoodY - 2, 4, {18, 18, 18, 255}, 1, 0);

        // 2. TECHO COMPACTO, VISERAS Y RETROVISOR PANORÁMICO
        // Techo ultra fino superior para dejar todo el parabrisas despejado
        drawNeobrutalRect(-20, -10 + swayY / 3, viewW + 40, 24, {22, 24, 28, 255}, 0, 0);
        renderThickLine(0, 14 + swayY / 3, viewW, 14 + swayY / 3, 2.0f, {220, 38, 38, 200});

        // Viseras parasol plegadas arriba
        renderPolygon(260 + swayX, 4 + swayY / 3, 800 + swayX, 4 + swayY / 3,
                      785 + swayX, 22 + swayY / 3, 280 + swayX, 22 + swayY / 3, {34, 36, 42, 255});
        renderPolygon(1120 + swayX, 4 + swayY / 3, 1660 + swayX, 4 + swayY / 3,
                      1640 + swayX, 22 + swayY / 3, 1140 + swayX, 22 + swayY / 3, {34, 36, 42, 255});

        // Espejo retrovisor central compacto de bajo perfil
        int mirrorW = 164;
        int mirrorH = 40;
        int mirrorX = (viewW / 2) - mirrorW / 2 + swayX;
        int mirrorY = 16 + swayY / 3;

        drawNeobrutalRect(mirrorX + mirrorW / 2 - 4, 6, 8, 12, {18, 18, 18, 255}, 1, 0);
        drawNeobrutalRect(mirrorX, mirrorY, mirrorW, mirrorH, {26, 28, 34, 255}, 3, 3);
        drawNeobrutalRect(mirrorX + 3, mirrorY + 3, mirrorW - 6, mirrorH - 6, {205, 210, 218, 255}, 1, 0);
        drawNeobrutalRect(mirrorX + 4, mirrorY + 4, mirrorW - 8, (mirrorH - 8) / 2, {244, 63, 94, 255}, 0, 0);
        drawNeobrutalRect(mirrorX + 4, mirrorY + 4 + (mirrorH - 8) / 2, mirrorW - 8, (mirrorH - 8) / 2, {34, 197, 94, 255}, 0, 0);

        int mMidX = mirrorX + mirrorW / 2 - static_cast<int>(car.getX() * 18.0f);
        int mMidY = mirrorY + 4 + (mirrorH - 8) / 2;
        renderPolygon(mMidX - 12, mMidY, mMidX + 12, mMidY, mMidX + 50, mirrorY + mirrorH - 4, mMidX - 50, mirrorY + mirrorH - 4, {39, 39, 42, 255});
        renderThickLine(mirrorX + 4, mMidY, mirrorX + mirrorW - 4, mMidY, 1.5f, {18, 18, 18, 255});

        // Reflejo del coche rival en el retrovisor
        if (otherCar) {
            float trackLen = road.getTrackLength();
            float relZ = otherCar->getTrackZ() - car.getTrackZ();
            while (relZ > trackLen * 0.5f) relZ -= trackLen;
            while (relZ < -trackLen * 0.5f) relZ += trackLen;

            if (relZ < -1.0f && relZ > -90.0f) {
                float distBehind = -relZ;
                float mirrorDistScale = std::clamp((90.0f - distBehind) / 90.0f, 0.2f, 1.0f);
                int rCarW = static_cast<int>(26 * mirrorDistScale);
                int rCarH = static_cast<int>(14 * mirrorDistScale);
                int rCarX = mirrorX + mirrorW / 2 + static_cast<int>((otherCar->getX() - car.getX()) * 26.0f);
                int rCarY = mirrorY + (mirrorH / 2) + static_cast<int>(distBehind * 0.12f);

                drawNeobrutalRect(rCarX - rCarW / 2, rCarY - rCarH / 2, rCarW, rCarH, otherColor, 1, 0);
                int fR = std::max(2, static_cast<int>(3 * mirrorDistScale));
                drawNeobrutalCircle(rCarX - rCarW / 3, rCarY, fR, {254, 240, 138, 255}, 1, 0);
                drawNeobrutalCircle(rCarX + rCarW / 3, rCarY, fR, {254, 240, 138, 255}, 1, 0);
            }
        }

        drawNeobrutalRect(mirrorX + mirrorW / 2 - 24, mirrorY + 4, 48, 14, {18, 18, 18, 230}, 1, 0);
        drawText(fontSmall_, "END", mirrorX + mirrorW / 2, mirrorY + 5, {74, 222, 128, 255}, true);

        // 3. PILAR A IZQUIERDO, VENTANILLA Y RETROVISOR LATERAL
        int aPillarBaseX = 220 + swayX;
        int aPillarBaseY = dashY;
        int doorSillY = dashY + 42;

        SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
        renderPolygon(-20, 20 + swayY / 3, 70 + swayX, 20 + swayY / 3, aPillarBaseX - 10, aPillarBaseY - 10, -20, doorSillY - 10, {15, 23, 42, 90});
        renderThickLine(-10, 80 + swayY / 3, 110 + swayX, 280 + swayY, 2.5f, {255, 255, 255, 75});

        renderPolygon(-30, -10, 80 + swayX, -10, aPillarBaseX + 25, aPillarBaseY, aPillarBaseX - 25, aPillarBaseY, {24, 26, 32, 255});
        renderThickLine(80 + swayX, -10, aPillarBaseX + 25, aPillarBaseY, 4.0f, {18, 18, 18, 255});
        renderThickLine(-30, -10, aPillarBaseX - 25, aPillarBaseY, 3.5f, {18, 18, 18, 255});

        renderPolygon(-20, doorSillY - 8, aPillarBaseX - 10, aPillarBaseY,
                      aPillarBaseX - 10, aPillarBaseY + 8, -20, doorSillY, {190, 45, 35, 255});
        renderThickLine(-20, doorSillY - 8, aPillarBaseX - 10, aPillarBaseY, 2.5f, {18, 18, 18, 255});

        // Espejo lateral izquierdo
        int mirX = 35 + swayX;
        int mirY = dashY - 65;
        int mirW = 75;
        int mirH = 75;
        renderPolygon(mirX + 25, mirY + mirH, mirX + 50, mirY + mirH,
                      mirX + 42, doorSillY - 2, mirX + 32, doorSillY - 2, {26, 28, 34, 255});
        drawNeobrutalRect(mirX, mirY, mirW, mirH, {26, 28, 34, 255}, 3, 3);
        drawNeobrutalRect(mirX + 3, mirY + 3, mirW - 6, mirH - 6, {205, 210, 218, 255}, 1, 0);
        drawNeobrutalRect(mirX + 5, mirY + 5, mirW - 10, mirH - 10, {15, 23, 42, 255}, 1, 0);
        drawNeobrutalRect(mirX + 6, mirY + 6, mirW - 12, (mirH - 12) / 2, {34, 197, 94, 255}, 0, 0);
        drawNeobrutalRect(mirX + 6, mirY + 6 + (mirH - 12) / 2, mirW - 12, (mirH - 12) / 2, {39, 42, 52, 255}, 0, 0);

        // Puerta interior izquierda
        renderPolygon(-20, doorSillY, aPillarBaseX + 15, aPillarBaseY,
                      aPillarBaseX + 15, aPillarBaseY + 22, -20, doorSillY + 28, {34, 38, 46, 255});
        renderPolygon(-20, doorSillY + 28, aPillarBaseX + 15, aPillarBaseY + 22,
                      aPillarBaseX + 15, aPillarBaseY + 58, -20, doorSillY + 65, {195, 200, 210, 255});
        renderPolygon(-20, doorSillY + 65, aPillarBaseX + 15, aPillarBaseY + 58,
                      aPillarBaseX + 15, viewH + 20, -20, viewH + 20, {26, 28, 34, 255});

        // Lado derecho (Pilar A del copiloto)
        int rDashEndY = dashY + 22;
        renderPolygon(viewW + 5, -10, viewW + 25, -10, viewW + 25, rDashEndY, viewW - 20 + swayX, rDashEndY, {26, 28, 34, 255});
        renderThickLine(viewW - 20 + swayX, rDashEndY, viewW - 5, -10, 3.5f, {18, 18, 18, 255});

        // 4. SALPICADERO PRINCIPAL Y MOLDURA DE TITANIO
        drawNeobrutalRect(aPillarBaseX, dashY, viewW - aPillarBaseX + 20, viewH - dashY + 30, {26, 28, 34, 255}, 5, 0);
        renderThickLine(aPillarBaseX, dashY + 8, viewW, dashY + 8, 2.5f, {220, 38, 38, 255});

        int trimH = 40;
        int trimY = dashY + 44;
        drawNeobrutalRect(aPillarBaseX + 10, trimY, viewW - aPillarBaseX - 25, trimH, {195, 200, 210, 255}, 3, 0);
        renderThickLine(aPillarBaseX + 10, trimY, viewW, trimY, 2.5f, {18, 18, 18, 255});
        renderThickLine(aPillarBaseX + 10, trimY + trimH, viewW, trimY + trimH, 2.5f, {18, 18, 18, 255});

        int lowerDashY = trimY + trimH;
        drawNeobrutalRect(aPillarBaseX, lowerDashY, viewW - aPillarBaseX + 20, viewH - lowerDashY + 30, {22, 24, 28, 255}, 4, 0);

        // Detalles del lado del acompañante (guantera y tobera de ventilación)
        drawNeobrutalRect(1380 + swayX, dashY + 28, 160, 22, {26, 28, 34, 255}, 2, 1);
        int ventX = viewW - 120 + swayX;
        int ventY = dashY + 36;
        drawNeobrutalCircle(ventX, ventY, 24, {26, 28, 34, 255}, 3, 2);
        drawNeobrutalCircle(ventX, ventY, 18, {205, 210, 218, 255}, 1, 0);

        // Hueco inferior de pedales
        int footwellX = 490 + swayX;
        int footwellY = dashY + 115;
        drawNeobrutalRect(footwellX, footwellY, 220, 80, {12, 14, 18, 255}, 3, 0);
        int brakeDepress = static_cast<int>(car.getBrakeInput() * 12.0f);
        int accelDepress = static_cast<int>(car.getThrottleInput() * 14.0f);
        drawNeobrutalRect(footwellX + 50, footwellY + 15 + brakeDepress, 36, 46 - brakeDepress / 2,
                          car.getBrakeInput() > 0.1f ? SDL_Color{239, 68, 68, 255} : SDL_Color{205, 210, 218, 255}, 2, 1);
        drawNeobrutalRect(footwellX + 120, footwellY + 10 + accelDepress, 30, 56 - accelDepress / 2,
                          car.getThrottleInput() > 0.1f ? SDL_Color{74, 222, 128, 255} : SDL_Color{205, 210, 218, 255}, 2, 1);

        // 5. CUADRO DE INSTRUMENTOS (BINNACLE) - CENTRADO FRENTE AL CONDUCTOR
        int binnacleX = 420 + swayX;
        int binnacleW = 450;
        int binnacleH = 115;
        int binnacleY = dashY - 82;

        drawNeobrutalRect(binnacleX, binnacleY, binnacleW, binnacleH, {138, 74, 44, 255}, 4, 4);
        drawNeobrutalRect(binnacleX + 8, binnacleY + 8, binnacleW - 16, binnacleH - 16, {96, 50, 30, 255}, 2, 0);

        // Barra LED de corte de inyección
        float rpmFrac = std::clamp((car.getRpm() - 1000.0f) / 6000.0f, 0.0f, 1.0f);
        for (int led = 0; led < 8; ++led) {
            float threshold = (led + 1) / 8.5f;
            bool active = rpmFrac >= threshold;
            SDL_Color ledCol = {38, 20, 12, 255};
            if (active) {
                if (led < 4) ledCol = {74, 222, 128, 255};
                else if (led < 6) ledCol = {250, 204, 21, 255};
                else ledCol = (static_cast<int>(raceTime * 20.0f) % 2 == 0) ? SDL_Color{239, 68, 68, 255} : SDL_Color{255, 255, 255, 255};
            }
            drawNeobrutalRect(binnacleX + 145 + led * 20, binnacleY + 12, 15, 6, ledCol, 1, 0);
        }

        // Relojes compactos del cuadro
        auto drawCompactGauge = [&](int gcx, int gcy, int radius, float valueFraction, const std::string& valText) {
            drawNeobrutalCircle(gcx, gcy, radius, {215, 220, 228, 255}, 2, 1);
            drawNeobrutalCircle(gcx, gcy, radius - 3, {16, 18, 22, 255}, 1, 0);

            float needleAngle = 2.356f + std::clamp(valueFraction, 0.0f, 1.0f) * 4.712f;
            int nx = gcx + static_cast<int>(std::cos(needleAngle) * (radius - 8));
            int ny = gcy + static_cast<int>(std::sin(needleAngle) * (radius - 8));
            renderThickLine(gcx, gcy, nx, ny, 2.5f, {239, 68, 68, 255});
            drawNeobrutalCircle(gcx, gcy, 4, {215, 220, 228, 255}, 1, 0);
            drawText(fontSmall_, valText, gcx, gcy - 8, {250, 204, 21, 255}, true);
        };

        char rpmBuf[16];
        std::snprintf(rpmBuf, sizeof(rpmBuf), "%.1fk", car.getRpm() / 1000.0f);
        drawCompactGauge(binnacleX + 78, binnacleY + 60, 38, rpmFrac, rpmBuf);

        float spdFrac = std::clamp(car.getSpeed() / 250.0f, 0.0f, 1.0f);
        std::string spdStr = std::to_string(static_cast<int>(car.getSpeed())) + "k";
        drawCompactGauge(binnacleX + 225, binnacleY + 58, 46, spdFrac, spdStr);

        float boostFrac = std::clamp(speedRatio * 0.85f + car.getThrottleInput() * 0.25f, 0.1f, 1.0f);
        std::string gearStr = "G" + std::to_string(car.getGear());
        drawCompactGauge(binnacleX + 372, binnacleY + 60, 38, boostFrac, gearStr);

        // 6. CONSOLA CENTRAL Y EQUALIZADOR
        int consoleX = 930 + swayX;
        int consoleY = dashY - 15;
        int consoleW = 270;
        int consoleH = 150;
        drawNeobrutalRect(consoleX, consoleY, consoleW, consoleH, {26, 28, 34, 255}, 4, 4);
        drawNeobrutalRect(consoleX + 10, consoleY + 10, consoleW - 20, consoleH - 20, {18, 20, 24, 255}, 2, 0);

        // Display de ecualizador
        drawNeobrutalRect(consoleX + 20, consoleY + 18, consoleW - 40, 36, {10, 12, 16, 255}, 2, 0);
        for (int bar = 0; bar < 10; ++bar) {
            float wave = std::abs(std::sin(raceTime * 7.5f + bar * 0.65f + speedRatio * 4.0f));
            int barH = std::clamp(static_cast<int>(3 + wave * 22.0f), 3, 26);
            SDL_Color barCol = (bar % 2 == 0) ? SDL_Color{56, 189, 248, 255} : SDL_Color{244, 63, 94, 255};
            drawNeobrutalRect(consoleX + 28 + bar * 22, consoleY + 48 - barH, 9, barH, barCol, 1, 0);
        }

        // Mandos inferiores de la consola (balizas y aire)
        drawNeobrutalRect(consoleX + 30, consoleY + 70, 32, 22, {239, 68, 68, 255}, 2, 1);
        drawNeobrutalRect(consoleX + 90, consoleY + 75, 140, 8, {10, 12, 16, 255}, 1, 0);
        drawNeobrutalCircle(consoleX + 140, consoleY + 79, 7, {205, 210, 218, 255}, 2, 0);

        // 7. PALANCA DE CAMBIOS DE REJILLA METÁLICA (H-GATE ANIMADA Y DE ALTO CONTRASTE)
        int shiftBaseX = 1055 + swayX;
        int shiftBaseY = dashY + 78;
        int gateW = 120;
        int gateH = 105;
        int gateX = shiftBaseX - gateW / 2;
        int gateY = shiftBaseY;

        // Base cromada con borde neobrutalista grueso
        drawNeobrutalRect(gateX, gateY, gateW, gateH, {245, 248, 252, 255}, 4, 5);
        drawNeobrutalRect(gateX + 10, gateY + 10, gateW - 20, gateH - 20, {18, 20, 26, 255}, 2, 0);

        // Ranuras talladas en H de la rejilla selectora
        int slotL_X = gateX + 24;
        int slotM_X = gateX + gateW / 2;
        int slotR_X = gateX + gateW - 24;
        renderThickLine(slotL_X, gateY + 22, slotL_X, gateY + gateH - 22, 3.5f, {245, 248, 252, 255});
        renderThickLine(slotM_X, gateY + 22, slotM_X, gateY + gateH - 22, 3.5f, {245, 248, 252, 255});
        renderThickLine(slotR_X, gateY + 22, slotR_X, gateY + gateH - 22, 3.5f, {245, 248, 252, 255});
        renderThickLine(gateX + 20, gateY + gateH / 2, gateX + gateW - 20, gateY + gateH / 2, 4.0f, {245, 248, 252, 255});

        // Posición física real del pomo según marcha seleccionada (R, N, 1ª a 6ª):
        int curGear = car.getGear();
        std::string gearText = car.getGearString();
        int targetSlotX = slotM_X;
        int targetSlotY = gateY + gateH / 2;
        switch (curGear) {
            case -1: targetSlotX = slotL_X - 10; targetSlotY = gateY + gateH / 2; break; // R (Izquierda)
            case  0: targetSlotX = slotM_X; targetSlotY = gateY + gateH / 2; break;      // N (Centro)
            case  1: targetSlotX = slotL_X; targetSlotY = gateY + 24; break;             // 1ª
            case  2: targetSlotX = slotL_X; targetSlotY = gateY + gateH - 24; break;     // 2ª
            case  3: targetSlotX = slotM_X; targetSlotY = gateY + 24; break;             // 3ª
            case  4: targetSlotX = slotM_X; targetSlotY = gateY + gateH - 24; break;     // 4ª
            case  5: targetSlotX = slotR_X; targetSlotY = gateY + 24; break;             // 5ª
            case  6: targetSlotX = slotR_X; targetSlotY = gateY + gateH - 24; break;     // 6ª
        }

        // Si hay animación de cambio activa, la palanca transiciona por el canal central
        float sAnim = car.getShiftAnim();
        int knobX = targetSlotX;
        int knobY = targetSlotY;
        if (sAnim > 0.01f) {
            float shiftProgress = std::sin(sAnim * 3.14159265f);
            knobX = static_cast<int>(targetSlotX * (1.0f - shiftProgress) + slotM_X * shiftProgress);
            knobY = static_cast<int>(targetSlotY * (1.0f - shiftProgress) + (gateY + gateH / 2) * shiftProgress);
        }

        int stickPivotX = shiftBaseX;
        int stickPivotY = gateY + gateH / 2 + 10;

        // Barra de cambio cromada inclinada hacia la posición de la marcha
        renderThickLine(stickPivotX, stickPivotY, knobX, knobY, 7.0f, {245, 248, 252, 255});

        // Pomo deportivo en Rojo Racing / Blanco de competición de alto contraste
        SDL_Color knobCol = (curGear == -1) ? SDL_Color{234, 179, 8, 255} : (curGear == 0 ? SDL_Color{148, 163, 184, 255} : SDL_Color{239, 68, 68, 255});
        drawNeobrutalCircle(knobX, knobY, 20, knobCol, 3, 3);
        drawNeobrutalCircle(knobX, knobY, 12, {255, 255, 255, 255}, 1, 0);
        drawText(fontSmall_, gearText, knobX, knobY - 9, {18, 18, 18, 255}, true);

        // 8. COLUMNA DE DIRECCIÓN Y VOLANTE DEPORTIVO
        int colX = 595 + swayX;
        int colY = dashY + 70;
        drawNeobrutalRect(colX, colY, 100, 100, {26, 28, 34, 255}, 3, 0);

        int wheelCenterX = 645 + swayX;
        int wheelCenterY = 415 + swayY;
        float wheelAngleDeg = car.getWheelVisualAngle();
        renderSteeringWheel(wheelCenterX, wheelCenterY, wheelAngleDeg, 0.58f);

        // 9. CONDUCTOR: BRAZOS, MANOS Y GUANTES DE RALLY REALISTAS
        float steerRad = wheelAngleDeg * 0.0174532925f;
        int rimMidRadius = 115; // Radio medio a escala 0.58f

        SDL_Color skinColor    = {226, 175, 142, 255};
        SDL_Color skinShadow   = {196, 142, 110, 255};
        SDL_Color gloveLeather = {24, 26, 32, 255};
        SDL_Color snapSilver   = {215, 220, 228, 255};

        // --- MANO Y BRAZO IZQUIERDO (Agarrando el volante a las 9:00) ---
        float leftGripAngle = kPi + steerRad;
        int handLX = wheelCenterX + static_cast<int>(std::cos(leftGripAngle) * rimMidRadius);
        int handLY = wheelCenterY + static_cast<int>(std::sin(leftGripAngle) * rimMidRadius);

        int armLRootX = 540 + swayX;
        int armLRootY = viewH + 20;

        // Vector director desde el codo hacia la muñeca
        float dirLX = static_cast<float>(handLX - armLRootX);
        float dirLY = static_cast<float>(handLY - armLRootY);
        float lenL = std::sqrt(dirLX * dirLX + dirLY * dirLY);
        if (lenL < 1.0f) lenL = 1.0f;
        dirLX /= lenL;
        dirLY /= lenL;
        float perpLX = -dirLY;
        float perpLY =  dirLX;

        int wristLX = handLX - static_cast<int>(dirLX * 18.0f);
        int wristLY = handLY - static_cast<int>(dirLY * 18.0f);

        // Antebrazo izquierdo (anatómico hacia la base del asiento)
        renderPolygon(armLRootX - static_cast<int>(perpLX * 32.0f), armLRootY - static_cast<int>(perpLY * 32.0f),
                      wristLX - static_cast<int>(perpLX * 14.0f),   wristLY - static_cast<int>(perpLY * 14.0f),
                      wristLX + static_cast<int>(perpLX * 14.0f),   wristLY + static_cast<int>(perpLY * 14.0f),
                      armLRootX + static_cast<int>(perpLX * 32.0f), armLRootY + static_cast<int>(perpLY * 32.0f),
                      skinColor);
        renderThickLine(armLRootX - static_cast<int>(perpLX * 32.0f), armLRootY - static_cast<int>(perpLY * 32.0f),
                        wristLX - static_cast<int>(perpLX * 14.0f),   wristLY - static_cast<int>(perpLY * 14.0f),
                        2.5f, skinShadow);

        // Guante de rally izquierdo
        drawNeobrutalRect(wristLX - 14, wristLY - 8, 28, 16, gloveLeather, 2, 1);
        drawNeobrutalCircle(handLX, handLY, 16, gloveLeather, 2, 1);
        drawNeobrutalCircle(handLX, handLY, 7, skinColor, 1, 0);
        drawNeobrutalCircle(wristLX - 4, wristLY, 2, snapSilver, 1, 0);

        // Dedos envolviendo el aro
        for (int f = -1; f <= 1; ++f) {
            int fx = handLX + static_cast<int>(perpLX * (f * 7.0f) + dirLX * 4.0f);
            int fy = handLY + static_cast<int>(perpLY * (f * 7.0f) + dirLY * 4.0f);
            drawNeobrutalCircle(fx, fy, 5, gloveLeather, 1, 0);
        }
        drawNeobrutalCircle(handLX - static_cast<int>(perpLX * 7.0f), handLY - static_cast<int>(perpLY * 7.0f), 5, gloveLeather, 1, 0);

        // --- MANO Y BRAZO DERECHO (Agarrando el volante a las 3:00 o cambiando de marcha) ---
        float rightGripAngle = 0.0f + steerRad;
        int handWheelRX = wheelCenterX + static_cast<int>(std::cos(rightGripAngle) * rimMidRadius);
        int handWheelRY = wheelCenterY + static_cast<int>(std::sin(rightGripAngle) * rimMidRadius);

        // Transición fluida entre volante y pomo de marchas
        float shiftWeight = std::sin(sAnim * 3.14159265f);
        int curHandRX = static_cast<int>(handWheelRX * (1.0f - shiftWeight) + knobX * shiftWeight);
        int curHandRY = static_cast<int>(handWheelRY * (1.0f - shiftWeight) + knobY * shiftWeight);

        int restArmRRootX = 750 + swayX;
        int shiftArmRRootX = knobX - 25;
        int armRRootX = static_cast<int>(restArmRRootX * (1.0f - shiftWeight) + shiftArmRRootX * shiftWeight);
        int armRRootY = viewH + 20;

        float dirRX = static_cast<float>(curHandRX - armRRootX);
        float dirRY = static_cast<float>(curHandRY - armRRootY);
        float lenR = std::sqrt(dirRX * dirRX + dirRY * dirRY);
        if (lenR < 1.0f) lenR = 1.0f;
        dirRX /= lenR;
        dirRY /= lenR;
        float perpRX = -dirRY;
        float perpRY =  dirRX;

        int wristRX = curHandRX - static_cast<int>(dirRX * 18.0f);
        int wristRY = curHandRY - static_cast<int>(dirRY * 18.0f);

        // Antebrazo derecho (anatómico y ergonómico, sin cruces ni distorsiones)
        renderPolygon(armRRootX - static_cast<int>(perpRX * 32.0f), armRRootY - static_cast<int>(perpRY * 32.0f),
                      wristRX - static_cast<int>(perpRX * 14.0f),   wristRY - static_cast<int>(perpRY * 14.0f),
                      wristRX + static_cast<int>(perpRX * 14.0f),   wristRY + static_cast<int>(perpRY * 14.0f),
                      armRRootX + static_cast<int>(perpRX * 32.0f), armRRootY + static_cast<int>(perpRY * 32.0f),
                      skinColor);
        renderThickLine(armRRootX + static_cast<int>(perpRX * 32.0f), armRRootY + static_cast<int>(perpRY * 32.0f),
                        wristRX + static_cast<int>(perpRX * 14.0f),   wristRY + static_cast<int>(perpRY * 14.0f),
                        2.5f, skinShadow);

        // Guante de rally derecho
        drawNeobrutalRect(wristRX - 14, wristRY - 8, 28, 16, gloveLeather, 2, 1);
        drawNeobrutalCircle(curHandRX, curHandRY, 16, gloveLeather, 2, 1);
        drawNeobrutalCircle(curHandRX, curHandRY, 7, skinColor, 1, 0);
        drawNeobrutalCircle(wristRX + 4, wristRY, 2, snapSilver, 1, 0);

        // Dedos envolviendo el aro / pomo
        for (int f = -1; f <= 1; ++f) {
            int fx = curHandRX + static_cast<int>(perpRX * (f * 7.0f) + dirRX * 4.0f);
            int fy = curHandRY + static_cast<int>(perpRY * (f * 7.0f) + dirRY * 4.0f);
            drawNeobrutalCircle(fx, fy, 5, gloveLeather, 1, 0);
        }
        drawNeobrutalCircle(curHandRX + static_cast<int>(perpRX * 7.0f), curHandRY + static_cast<int>(perpRY * 7.0f), 5, gloveLeather, 1, 0);
    } else {
        // =========================================================================
        // CABINA EN PANTALLA COMPLETA 1P (1920x1080)
        // ESTILO MANGA/INITIAL D NEOBRUTALISTA DE MÁXIMO DETALLE
        // =========================================================================
        float speedRatio = car.getSpeedRatio();
        float bobAmp = (1.2f + speedRatio * 3.8f) + (car.isOffRoad() ? 6.5f : 0.0f);

        // Desplazamiento orgánico de cabina por fuerzas G laterales y cabeceo (cámara sigue la inercia del piloto)
        float curSegCurve = road.findSegment(car.getTrackZ()).curve;
        int swayX = static_cast<int>(-car.getRoll() * 5.2f - curSegCurve * 4.5f + std::cos(car.getSuspensionBob() * 0.5f) * (bobAmp * 0.6f));
        int swayY = static_cast<int>( car.getPitch() * 1.6f + std::sin(car.getSuspensionBob()) * bobAmp);

        // 0. CAPÓ EXTERIOR VISIBLE A TRAVÉS DEL PARABRISAS
        int hoodY = 645 + swayY;
        renderPolygon(240 + swayX, hoodY + 15, 480 + swayX, hoodY + 15,
                      460 + swayX, hoodY + 45, 200 + swayX, hoodY + 45, {190, 45, 35, 255});
        renderThickLine(240 + swayX, hoodY + 15, 480 + swayX, hoodY + 15, 3.5f, {18, 18, 18, 255});

        // Limpiaparabrisas dobles negros reposando en la base del cristal
        renderThickLine(520 + swayX, hoodY - 3, 940 + swayX, hoodY - 19, 5.0f, {24, 26, 30, 255});
        renderThickLine(980 + swayX, hoodY - 3, 1460 + swayX, hoodY - 17, 5.0f, {24, 26, 30, 255});
        drawNeobrutalCircle(520 + swayX, hoodY - 3, 6, {18, 18, 18, 255}, 2, 0);
        drawNeobrutalCircle(980 + swayX, hoodY - 3, 6, {18, 18, 18, 255}, 2, 0);

        // 1. SALPICADERO PRINCIPAL (DASHBOARD) Y PEDALERA INFERIOR
        int dashY = 648 + swayY;
        int aPillarBaseX = 220 + swayX;
        int aPillarBaseY = dashY; 
        int doorSillY    = dashY + 68;

        drawNeobrutalRect(aPillarBaseX, dashY, viewW - aPillarBaseX + 20, viewH - dashY + 40, {26, 28, 34, 255}, 6, 0);
        renderThickLine(aPillarBaseX, dashY + 12, viewW, dashY + 12, 3.0f, {220, 38, 38, 255});

        int trimH = 68;
        drawNeobrutalRect(aPillarBaseX + 15, dashY + 70, viewW - aPillarBaseX - 35, trimH, {195, 200, 210, 255}, 4, 0);
        renderThickLine(aPillarBaseX + 15, dashY + 70, viewW, dashY + 70, 3.5f, {18, 18, 18, 255});
        renderThickLine(aPillarBaseX + 15, dashY + 70 + trimH, viewW, dashY + 70 + trimH, 3.5f, {18, 18, 18, 255});

        int lowerDashY = dashY + 70 + trimH;
        drawNeobrutalRect(aPillarBaseX, lowerDashY, viewW - aPillarBaseX + 20, viewH - lowerDashY + 40, {22, 24, 28, 255}, 5, 0);

        int footwellX = 520 + swayX;
        int footwellY = dashY + 235;
        drawNeobrutalRect(footwellX, footwellY, 360, 220, {12, 14, 18, 255}, 4, 0);
        int brakeDepress = static_cast<int>(car.getBrakeInput() * 18.0f);
        int accelDepress = static_cast<int>(car.getThrottleInput() * 22.0f);
        drawNeobrutalRect(footwellX + 95, footwellY + 35 + brakeDepress,
                          58, 72 - brakeDepress / 2,
                          car.getBrakeInput() > 0.1f ? SDL_Color{239, 68, 68, 255} : SDL_Color{205, 210, 218, 255}, 3, 2);
        drawNeobrutalRect(footwellX + 205, footwellY + 25 + accelDepress,
                          48, 95 - accelDepress / 2,
                          car.getThrottleInput() > 0.1f ? SDL_Color{74, 222, 128, 255} : SDL_Color{205, 210, 218, 255}, 3, 2);

        // 2. PILAR A IZQUIERDO, VENTANILLA Y PUERTA
        SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
        renderPolygon(-40, 60 + swayY, 70 + swayX, 60 + swayY, aPillarBaseX - 10, aPillarBaseY - 20, -40, doorSillY - 20, {15, 23, 42, 90});
        renderThickLine(-10, 160 + swayY, 130 + swayX, 480 + swayY, 3.0f, {255, 255, 255, 75});

        renderPolygon(-40, -20, 80 + swayX, -20, aPillarBaseX + 35, aPillarBaseY, aPillarBaseX - 25, aPillarBaseY, {24, 26, 32, 255});
        renderThickLine(80 + swayX, -10, aPillarBaseX + 35, aPillarBaseY, 5.0f, {18, 18, 18, 255});
        renderThickLine(-40, -10, aPillarBaseX - 25, aPillarBaseY, 4.0f, {18, 18, 18, 255});

        renderPolygon(-20, doorSillY - 14, aPillarBaseX - 10, aPillarBaseY,
                      aPillarBaseX - 10, aPillarBaseY + 12, -20, doorSillY - 2, {190, 45, 35, 255});
        renderThickLine(-20, doorSillY - 14, aPillarBaseX - 10, aPillarBaseY, 3.5f, {18, 18, 18, 255});

        // Espejo retrovisor exterior izquierdo
        int mirX = 20 + swayX;
        int mirY = 465 + swayY;
        int mirW = 105;
        int mirH = 115;
        renderPolygon(mirX + 35, mirY + mirH, mirX + 70, mirY + mirH,
                      mirX + 60, doorSillY - 4, mirX + 45, doorSillY - 4, {26, 28, 34, 255});
        drawNeobrutalRect(mirX, mirY, mirW, mirH, {26, 28, 34, 255}, 4, 4);
        drawNeobrutalRect(mirX + 4, mirY + 4, mirW - 8, mirH - 8, {205, 210, 218, 255}, 2, 0);
        drawNeobrutalRect(mirX + 7, mirY + 7, mirW - 14, mirH - 14, {15, 23, 42, 255}, 1, 0);
        drawNeobrutalRect(mirX + 8, mirY + 8, mirW - 16, (mirH - 16) * 50 / 100, {34, 197, 94, 255}, 0, 0);
        drawNeobrutalRect(mirX + 8, mirY + 8 + (mirH - 16) * 50 / 100, mirW - 16, (mirH - 16) * 50 / 100, {39, 42, 52, 255}, 0, 0);

        // Panel interior de la puerta izquierda
        renderPolygon(-40, doorSillY, aPillarBaseX + 22, aPillarBaseY,
                      aPillarBaseX + 22, aPillarBaseY + 35, -40, doorSillY + 45, {34, 38, 46, 255});
        renderPolygon(-40, doorSillY + 45, aPillarBaseX + 22, aPillarBaseY + 35,
                      aPillarBaseX + 22, aPillarBaseY + 95, -40, doorSillY + 105, {195, 200, 210, 255});
        renderPolygon(-40, doorSillY + 105, aPillarBaseX + 22, aPillarBaseY + 95,
                      aPillarBaseX + 22, viewH + 20, -40, viewH + 20, {26, 28, 34, 255});

        // Lado derecho del habitáculo
        int rDashEndX = viewW - 25 + swayX;
        int rDashEndY = dashY + 35;
        renderPolygon(viewW + 5, -20, viewW + 30, -20,
                      viewW + 30, rDashEndY, rDashEndX, rDashEndY, {26, 28, 34, 255});
        renderThickLine(rDashEndX, rDashEndY, viewW - 5, -20, 4.0f, {18, 18, 18, 255});
        drawNeobrutalRect(viewW - 75 + swayX, rDashEndY + 80, 85, viewH - (rDashEndY + 80) + 40, {26, 28, 34, 255}, 4, 0);

        // Techo interior y Viseras Parasol
        drawNeobrutalRect(-30, -20 + swayY / 2, viewW + 60, 105, {22, 24, 28, 255}, 5, 0);

        int visorY = 78 + swayY / 2;
        int visorH = 118;
        renderPolygon(220 + swayX, visorY, 825 + swayX, visorY, 795 + swayX, visorY + visorH, 250 + swayX, visorY + visorH, {32, 34, 40, 255});
        renderPolygon(1095 + swayX, visorY, viewW - 160 + swayX, visorY, viewW - 190 + swayX, visorY + visorH, 1125 + swayX, visorY + visorH, {32, 34, 40, 255});

        // Espejo retrovisor panorámico central
        int mirrorW = 210;
        int mirrorH = 74;
        int mirrorX = (viewW / 2) - mirrorW / 2 + swayX;
        int mirrorY = visorY + 42;
        drawNeobrutalRect(mirrorX + mirrorW / 2 - 6, visorY - 4, 12, 50, {18, 18, 18, 255}, 2, 0);
        drawNeobrutalRect(mirrorX, mirrorY, mirrorW, mirrorH, {26, 28, 34, 255}, 5, 6);
        drawNeobrutalRect(mirrorX + 4, mirrorY + 4, mirrorW - 8, mirrorH - 8, {205, 210, 218, 255}, 2, 0);
        drawNeobrutalRect(mirrorX + 6, mirrorY + 6, mirrorW - 12, (mirrorH - 12) / 2, {244, 63, 94, 255}, 0, 0);
        drawNeobrutalRect(mirrorX + 6, mirrorY + 6 + (mirrorH - 12) / 2, mirrorW - 12, (mirrorH - 12) / 2, {34, 197, 94, 255}, 0, 0);

        int mMidX = mirrorX + mirrorW / 2 - static_cast<int>(car.getX() * 22.0f);
        int mMidY = mirrorY + 6 + (mirrorH - 12) / 2;
        renderPolygon(mMidX - 18, mMidY, mMidX + 18, mMidY, mMidX + 75, mirrorY + mirrorH - 6, mMidX - 75, mirrorY + mirrorH - 6, {39, 39, 42, 255});
        renderThickLine(mirrorX + 6, mMidY, mirrorX + mirrorW - 6, mMidY, 2.0f, {18, 18, 18, 255});

        drawNeobrutalRect(mirrorX + mirrorW / 2 - 32, mirrorY + 8, 64, 18, {18, 18, 18, 230}, 2, 0);
        drawText(fontSmall_, "END", mirrorX + mirrorW / 2, mirrorY + 10, {74, 222, 128, 255}, true);

        // 3. CUADRO DE INSTRUMENTOS (BINNACLE)
        int binnacleW = 590;
        int binnacleH = 225;
        int binnacleX = 410 + swayX;
        int binnacleY = dashY - 145;

        drawNeobrutalRect(binnacleX, binnacleY, binnacleW, binnacleH, {138, 74, 44, 255}, 5, 6);
        drawNeobrutalRect(binnacleX + 12, binnacleY + 12, binnacleW - 24, binnacleH - 24, {96, 50, 30, 255}, 3, 0);

        float rpmFrac = std::clamp((car.getRpm() - 1000.0f) / 6000.0f, 0.0f, 1.0f);
        for (int led = 0; led < 8; ++led) {
            float threshold = (led + 1) / 8.5f;
            bool active = rpmFrac >= threshold;
            SDL_Color ledCol = {38, 20, 12, 255};
            if (active) {
                if (led < 4) ledCol = {74, 222, 128, 255};
                else if (led < 6) ledCol = {250, 204, 21, 255};
                else ledCol = (static_cast<int>(raceTime * 20.0f) % 2 == 0) ? SDL_Color{239, 68, 68, 255} : SDL_Color{255, 255, 255, 255};
            }
            drawNeobrutalRect(binnacleX + 168 + led * 32, binnacleY + 18, 20, 8, ledCol, 1, 0);
        }

        auto drawGauge = [&](int gcx, int gcy, int radius, float valueFraction, const std::string& label, const std::string& numText) {
            drawNeobrutalCircle(gcx, gcy, radius, {215, 220, 228, 255}, 3, 2);
            drawNeobrutalCircle(gcx, gcy, radius - 4, {16, 18, 22, 255}, 1, 0);

            float needleAngle = 2.356f + std::clamp(valueFraction, 0.0f, 1.0f) * 4.712f;
            int nx = gcx + static_cast<int>(std::cos(needleAngle) * (radius - 12));
            int ny = gcy + static_cast<int>(std::sin(needleAngle) * (radius - 12));
            renderThickLine(gcx, gcy, nx, ny, 3.5f, {239, 68, 68, 255});
            drawNeobrutalCircle(gcx, gcy, 8, {215, 220, 228, 255}, 1, 0);

            drawText(fontSmall_, label, gcx, gcy - radius / 2 - 4, {209, 213, 219, 255}, true);
            drawText(fontSmall_, numText, gcx, gcy + radius / 2 - 12, {250, 204, 21, 255}, true);
        };

        char rpmBuf[16];
        std::snprintf(rpmBuf, sizeof(rpmBuf), "%.1fk", car.getRpm() / 1000.0f);
        drawGauge(binnacleX + 102, binnacleY + binnacleH / 2 + 6, 60, rpmFrac, "RPM", rpmBuf);

        float spdFrac = std::clamp(car.getSpeed() / 250.0f, 0.0f, 1.0f);
        std::string spdStr = std::to_string(static_cast<int>(car.getSpeed())) + "k";
        drawGauge(binnacleX + 295, binnacleY + binnacleH / 2 + 4, 74, spdFrac, "SPEED", spdStr);

        float boostFrac = std::clamp(speedRatio * 0.85f + car.getThrottleInput() * 0.25f, 0.1f, 1.0f);
        std::string gearStr = (car.getGear() > 0) ? ("G" + std::to_string(car.getGear())) : car.getGearString();
        drawGauge(binnacleX + 488, binnacleY + binnacleH / 2 + 6, 60, boostFrac, "GEAR", gearStr);

        // 4. CONSOLA CENTRAL
        int consoleX = 1040 + swayX;
        int consoleY = dashY - 40;
        int consoleW = 340;
        int consoleH = 240;
        drawNeobrutalRect(consoleX, consoleY, consoleW, consoleH, {26, 28, 34, 255}, 5, 6);
        drawNeobrutalRect(consoleX + 14, consoleY + 14, consoleW - 28, consoleH - 28, {18, 20, 24, 255}, 3, 0);

        // Ecualizador
        drawNeobrutalRect(consoleX + 30, consoleY + 28, consoleW - 60, 44, {10, 12, 16, 255}, 2, 0);
        for (int bar = 0; bar < 10; ++bar) {
            float wave = std::abs(std::sin(raceTime * 7.5f + bar * 0.65f + speedRatio * 4.0f));
            int barH = std::clamp(static_cast<int>(4 + wave * 26.0f), 3, 32);
            SDL_Color barCol = (bar % 2 == 0) ? SDL_Color{56, 189, 248, 255} : SDL_Color{244, 63, 94, 255};
            drawNeobrutalRect(consoleX + 38 + bar * 18, consoleY + 66 - barH, 12, barH, barCol, 1, 0);
        }

        // 5. TOBERAS DE AIRE ACONDICIONADO
        int grabX = 1410 + swayX;
        int grabY = dashY + 45;
        drawNeobrutalRect(grabX, grabY, 210, 36, {26, 28, 34, 255}, 3, 2);

        int ventX = viewW - 230 + swayX;
        int ventY = dashY + 62;
        drawNeobrutalCircle(ventX, ventY, 38, {26, 28, 34, 255}, 3, 2);
        drawNeobrutalCircle(ventX, ventY, 30, {205, 210, 218, 255}, 1, 0);

        // 6. COLUMNA DE DIRECCIÓN Y PALANCA DE CAMBIOS
        int colX = 620 + swayX;
        int colY = dashY + 140;
        drawNeobrutalRect(colX, colY, 140, 160, {26, 28, 34, 255}, 3, 0);

        int shiftBaseX = 1130 + swayX;
        int shiftBaseY = dashY + 230;
        int gateW = 160;
        int gateH = 150;
        int gateX = shiftBaseX - gateW / 2;
        int gateY = shiftBaseY;

        drawNeobrutalRect(gateX, gateY, gateW, gateH, {245, 248, 252, 255}, 4, 6);
        drawNeobrutalRect(gateX + 14, gateY + 14, gateW - 28, gateH - 28, {18, 20, 26, 255}, 2, 0);

        // Ranuras talladas en H
        int slotL_X = gateX + 32;
        int slotM_X = gateX + gateW / 2;
        int slotR_X = gateX + gateW - 32;
        renderThickLine(slotL_X, gateY + 28, slotL_X, gateY + gateH - 28, 4.5f, {245, 248, 252, 255});
        renderThickLine(slotM_X, gateY + 28, slotM_X, gateY + gateH - 28, 4.5f, {245, 248, 252, 255});
        renderThickLine(slotR_X, gateY + 28, slotR_X, gateY + gateH - 28, 4.5f, {245, 248, 252, 255});
        renderThickLine(gateX + 28, gateY + gateH / 2, gateX + gateW - 28, gateY + gateH / 2, 5.5f, {245, 248, 252, 255});

        // Posición física real del pomo según marcha seleccionada (R, N, 1ª a 6ª):
        int curGear = car.getGear();
        std::string gearText = car.getGearString();
        int targetSlotX = slotM_X;
        int targetSlotY = gateY + gateH / 2;
        switch (curGear) {
            case -1: targetSlotX = slotL_X - 12; targetSlotY = gateY + gateH / 2; break; // R (Empujado a la izquierda)
            case  0: targetSlotX = slotM_X; targetSlotY = gateY + gateH / 2; break;      // N (Centro)
            case  1: targetSlotX = slotL_X; targetSlotY = gateY + 30; break;             // 1ª
            case  2: targetSlotX = slotL_X; targetSlotY = gateY + gateH - 30; break;     // 2ª
            case  3: targetSlotX = slotM_X; targetSlotY = gateY + 30; break;             // 3ª
            case  4: targetSlotX = slotM_X; targetSlotY = gateY + gateH - 30; break;     // 4ª
            case  5: targetSlotX = slotR_X; targetSlotY = gateY + 30; break;             // 5ª
            case  6: targetSlotX = slotR_X; targetSlotY = gateY + gateH - 30; break;     // 6ª
        }

        float sAnim = car.getShiftAnim();
        int knobX = targetSlotX;
        int knobY = targetSlotY;
        if (sAnim > 0.01f) {
            float shiftProgress = std::sin(sAnim * 3.14159265f);
            knobX = static_cast<int>(targetSlotX * (1.0f - shiftProgress) + slotM_X * shiftProgress);
            knobY = static_cast<int>(targetSlotY * (1.0f - shiftProgress) + (gateY + gateH / 2) * shiftProgress);
        }

        int stickPivotX = shiftBaseX;
        int stickPivotY = gateY + gateH / 2 + 15;

        renderThickLine(stickPivotX, stickPivotY, knobX, knobY, 9.0f, {245, 248, 252, 255});
        SDL_Color knobCol = (curGear == -1) ? SDL_Color{234, 179, 8, 255} : (curGear == 0 ? SDL_Color{148, 163, 184, 255} : SDL_Color{239, 68, 68, 255});
        if (car.isStalled()) knobCol = {185, 28, 28, 255}; // Alerta de calado

        drawNeobrutalCircle(knobX, knobY, 26, knobCol, 3, 3);
        drawNeobrutalCircle(knobX, knobY, 16, {255, 255, 255, 255}, 1, 0);
        drawText(fontSmall_, gearText, knobX, knobY - 10, {18, 18, 18, 255}, true);

        // Alerta visual de motor calado (Stalled)
        if (car.isStalled()) {
            drawNeobrutalRect(shiftBaseX - 70, gateY - 32, 140, 24, {220, 38, 38, 255}, 2, 2);
            drawText(fontSmall_, "! CALADO !", shiftBaseX, gateY - 30, {255, 255, 255, 255}, true);
        }

        // 7. VOLANTE DEPORTIVO PERFORADO
        int wheelCenterX = 680 + swayX;
        int wheelCenterY = 825 + swayY;
        float wheelAngleDeg = car.getWheelVisualAngle();
        renderSteeringWheel(wheelCenterX, wheelCenterY, wheelAngleDeg, 1.0f);

        // 8. MANOS Y GUANTES DEL CONDUCTOR (1080p)
        float steerRad = wheelAngleDeg * 0.0174532925f;
        int wheelRadius = 215;
        int rimThickness = 32;
        int rimMidRadius = wheelRadius - rimThickness / 2;

        SDL_Color skinColor     = {226, 175, 142, 255};
        SDL_Color skinShadow    = {196, 142, 110, 255};
        SDL_Color gloveLeather  = {24, 26, 32, 255};
        SDL_Color snapSilver    = {215, 220, 228, 255};

        float leftGripAngle = kPi + steerRad;
        int handLX = wheelCenterX + static_cast<int>(std::cos(leftGripAngle) * rimMidRadius);
        int handLY = wheelCenterY + static_cast<int>(std::sin(leftGripAngle) * rimMidRadius);

        int armLRootX = 420 + swayX;
        int armLRootY = viewH + 25;

        float dirLX = static_cast<float>(handLX - armLRootX);
        float dirLY = static_cast<float>(handLY - armLRootY);
        float lenL = std::sqrt(dirLX * dirLX + dirLY * dirLY);
        if (lenL < 1.0f) lenL = 1.0f;
        dirLX /= lenL;
        dirLY /= lenL;
        float perpLX = -dirLY;
        float perpLY =  dirLX;

        int wristLX = handLX - static_cast<int>(dirLX * 24.0f);
        int wristLY = handLY - static_cast<int>(dirLY * 24.0f);

        renderPolygon(armLRootX - static_cast<int>(perpLX * 45.0f), armLRootY - static_cast<int>(perpLY * 45.0f),
                      wristLX - static_cast<int>(perpLX * 20.0f),   wristLY - static_cast<int>(perpLY * 20.0f),
                      wristLX + static_cast<int>(perpLX * 20.0f),   wristLY + static_cast<int>(perpLY * 20.0f),
                      armLRootX + static_cast<int>(perpLX * 45.0f), armLRootY + static_cast<int>(perpLY * 45.0f),
                      skinColor);
        renderThickLine(armLRootX - static_cast<int>(perpLX * 45.0f), armLRootY - static_cast<int>(perpLY * 45.0f),
                        wristLX - static_cast<int>(perpLX * 20.0f),   wristLY - static_cast<int>(perpLY * 20.0f),
                        3.0f, skinShadow);

        drawNeobrutalRect(wristLX - 24, wristLY - 12, 48, 24, gloveLeather, 2, 1);
        drawNeobrutalCircle(handLX, handLY, 26, gloveLeather, 3, 2);
        drawNeobrutalCircle(handLX, handLY, 12, skinColor, 2, 0);
        drawNeobrutalCircle(wristLX - 8, wristLY, 3, snapSilver, 1, 0);

        for (int f = -1; f <= 1; ++f) {
            int fx = handLX + static_cast<int>(perpLX * (f * 12.0f) + dirLX * 6.0f);
            int fy = handLY + static_cast<int>(perpLY * (f * 12.0f) + dirLY * 6.0f);
            drawNeobrutalCircle(fx, fy, 8, gloveLeather, 2, 0);
        }
        drawNeobrutalCircle(handLX - static_cast<int>(perpLX * 10.0f), handLY - static_cast<int>(perpLY * 10.0f), 8, gloveLeather, 2, 0);

        float shiftWeight = std::sin(sAnim * 3.14159265f);
        float rightGripAngle = 0.0f + steerRad;
        int handWheelRX = wheelCenterX + static_cast<int>(std::cos(rightGripAngle) * rimMidRadius);
        int handWheelRY = wheelCenterY + static_cast<int>(std::sin(rightGripAngle) * rimMidRadius);

        int curHandRX = static_cast<int>(handWheelRX * (1.0f - shiftWeight) + knobX * shiftWeight);
        int curHandRY = static_cast<int>(handWheelRY * (1.0f - shiftWeight) + (knobY + 2) * shiftWeight);

        int restArmRRootX = 940 + swayX;
        int shiftArmRRootX = knobX - 30;
        int armRRootX = static_cast<int>(restArmRRootX * (1.0f - shiftWeight) + shiftArmRRootX * shiftWeight);
        int armRRootY = viewH + 25;

        float dirRX = static_cast<float>(curHandRX - armRRootX);
        float dirRY = static_cast<float>(curHandRY - armRRootY);
        float lenR = std::sqrt(dirRX * dirRX + dirRY * dirRY);
        if (lenR < 1.0f) lenR = 1.0f;
        dirRX /= lenR;
        dirRY /= lenR;
        float perpRX = -dirRY;
        float perpRY =  dirRX;

        int wristRX = curHandRX - static_cast<int>(dirRX * 24.0f);
        int wristRY = curHandRY - static_cast<int>(dirRY * 24.0f);

        renderPolygon(armRRootX - static_cast<int>(perpRX * 45.0f), armRRootY - static_cast<int>(perpRY * 45.0f),
                      wristRX - static_cast<int>(perpRX * 20.0f),   wristRY - static_cast<int>(perpRY * 20.0f),
                      wristRX + static_cast<int>(perpRX * 20.0f),   wristRY + static_cast<int>(perpRY * 20.0f),
                      armRRootX + static_cast<int>(perpRX * 45.0f), armRRootY + static_cast<int>(perpRY * 45.0f),
                      skinColor);
        renderThickLine(armRRootX + static_cast<int>(perpRX * 45.0f), armRRootY + static_cast<int>(perpRY * 45.0f),
                        wristRX + static_cast<int>(perpRX * 20.0f),   wristRY + static_cast<int>(perpRY * 20.0f),
                        3.0f, skinShadow);

        drawNeobrutalRect(wristRX - 24, wristRY - 12, 48, 24, gloveLeather, 2, 1);
        drawNeobrutalCircle(curHandRX, curHandRY, 26, gloveLeather, 3, 2);
        drawNeobrutalCircle(curHandRX, curHandRY, 12, skinColor, 2, 0);
        drawNeobrutalCircle(wristRX + 8, wristRY, 3, snapSilver, 1, 0);

        for (int f = -1; f <= 1; ++f) {
            int fx = curHandRX + static_cast<int>(perpRX * (f * 12.0f) + dirRX * 6.0f);
            int fy = curHandRY + static_cast<int>(perpRY * (f * 12.0f) + dirRY * 6.0f);
            drawNeobrutalCircle(fx, fy, 8, gloveLeather, 2, 0);
        }
        drawNeobrutalCircle(curHandRX + static_cast<int>(perpRX * 10.0f), curHandRY + static_cast<int>(perpRY * 10.0f), 8, gloveLeather, 2, 0);
    }
}

void Renderer::renderSteeringWheel(int cx, int cy, float angleDeg, float scale) {
    float rad = angleDeg * 0.0174532925f;
    int wheelRadius = static_cast<int>(215 * scale);
    int rimThickness = static_cast<int>(32 * scale);

    // Aro exterior hueco poligonal con SDL_RenderGeometry en cuero negro deportivo
    const int segments = 40;
    std::vector<SDL_Vertex> ringVerts(segments * 2);
    std::vector<int> ringIndices(segments * 6);

    SDL_Color rimOuterColor = {24, 26, 32, 255}; // Cuero negro exterior
    SDL_Color rimInnerColor = {42, 46, 56, 255}; // Sombra y costura interior

    for (int i = 0; i < segments; ++i) {
        float a = (static_cast<float>(i) / segments) * 2.0f * kPi;
        float cosA = std::cos(a);
        float sinA = std::sin(a);

        // Vértice exterior
        ringVerts[i * 2 + 0].position = {cx + cosA * wheelRadius, cy + sinA * wheelRadius};
        ringVerts[i * 2 + 0].color = rimOuterColor;
        ringVerts[i * 2 + 0].tex_coord = {0, 0};

        // Vértice interior
        ringVerts[i * 2 + 1].position = {cx + cosA * (wheelRadius - rimThickness), cy + sinA * (wheelRadius - rimThickness)};
        ringVerts[i * 2 + 1].color = rimInnerColor;
        ringVerts[i * 2 + 1].tex_coord = {0, 0};

        int nextI = (i + 1) % segments;
        int idx = i * 6;
        ringIndices[idx + 0] = i * 2 + 0;
        ringIndices[idx + 1] = nextI * 2 + 0;
        ringIndices[idx + 2] = i * 2 + 1;

        ringIndices[idx + 3] = i * 2 + 1;
        ringIndices[idx + 4] = nextI * 2 + 0;
        ringIndices[idx + 5] = nextI * 2 + 1;
    }
    SDL_RenderGeometry(ren_, nullptr, ringVerts.data(), static_cast<int>(ringVerts.size()),
                       ringIndices.data(), static_cast<int>(ringIndices.size()));

    // Contornos exterior e interior del aro
    for (int i = 0; i < segments; ++i) {
        float a1 = (static_cast<float>(i) / segments) * 2.0f * kPi;
        float a2 = (static_cast<float>((i + 1) % segments) / segments) * 2.0f * kPi;
        renderThickLine(cx + std::cos(a1) * wheelRadius, cy + std::sin(a1) * wheelRadius,
                        cx + std::cos(a2) * wheelRadius, cy + std::sin(a2) * wheelRadius, 3.5f * scale, {18, 18, 18, 255});
        renderThickLine(cx + std::cos(a1) * (wheelRadius - rimThickness), cy + std::sin(a1) * (wheelRadius - rimThickness),
                        cx + std::cos(a2) * (wheelRadius - rimThickness), cy + std::sin(a2) * (wheelRadius - rimThickness), 3.0f * scale, {18, 18, 18, 255});
    }

    // Marca de centrado superior en el aro (Top center stripe roja de rally)
    float topMarkAngle = rad - kPi * 0.5f;
    int mx = cx + static_cast<int>(std::cos(topMarkAngle) * (wheelRadius - rimThickness / 2));
    int my = cy + static_cast<int>(std::sin(topMarkAngle) * (wheelRadius - rimThickness / 2));
    drawNeobrutalCircle(mx, my, std::max(6, static_cast<int>(12 * scale)), {239, 68, 68, 255}, 2, 0);

    // 2. Núcleo central del volante: disco de aluminio cepillado con botón de bocina negro y 6 tornillos
    int hubR = static_cast<int>(56 * scale);
    drawNeobrutalCircle(cx, cy, hubR, {205, 210, 218, 255}, std::max(3, static_cast<int>(5 * scale)), static_cast<int>(4 * scale));
    drawNeobrutalCircle(cx, cy, static_cast<int>(30 * scale), {18, 18, 18, 255}, 3, 0); // Bocina central negra
    drawNeobrutalCircle(cx, cy, static_cast<int>(14 * scale), {28, 30, 36, 255}, 2, 0);

    for (int t = 0; t < 6; ++t) {
        float bAngle = rad + t * (kPi / 3.0f);
        int bx = cx + static_cast<int>(std::cos(bAngle) * (42 * scale));
        int by = cy + static_cast<int>(std::sin(bAngle) * (42 * scale));
        drawNeobrutalCircle(bx, by, std::max(2, static_cast<int>(4 * scale)), {18, 18, 18, 255}, 1, 0);
    }

    // 3. Los 3 Radios Metálicos Perforados en aluminio cepillado (estilo Nardi / Momo clásico de la referencia)
    auto drawPerforatedSpoke = [&](float spokeBaseAngle) {
        float totalA = rad + spokeBaseAngle;
        float cosA = std::cos(totalA);
        float sinA = std::sin(totalA);

        float perpX = -sinA * (22.0f * scale);
        float perpY = cosA * (22.0f * scale);

        int x1 = cx + static_cast<int>(cosA * (hubR - 4) + perpX);
        int y1 = cy + static_cast<int>(sinA * (hubR - 4) + perpY);
        int x2 = cx + static_cast<int>(cosA * (hubR - 4) - perpX);
        int y2 = cy + static_cast<int>(sinA * (hubR - 4) - perpY);
        int x3 = cx + static_cast<int>(cosA * (wheelRadius - rimThickness + 2) - perpX * 0.7f);
        int y3 = cy + static_cast<int>(sinA * (wheelRadius - rimThickness + 2) - perpY * 0.7f);
        int x4 = cx + static_cast<int>(cosA * (wheelRadius - rimThickness + 2) + perpX * 0.7f);
        int y4 = cy + static_cast<int>(sinA * (wheelRadius - rimThickness + 2) + perpY * 0.7f);

        renderPolygon(x1, y1, x2, y2, x3, y3, x4, y4, {205, 210, 218, 255});
        renderThickLine(x1, y1, x4, y4, 3.5f * scale, {18, 18, 18, 255});
        renderThickLine(x2, y2, x3, y3, 3.5f * scale, {18, 18, 18, 255});

        // 3 agujeros circulares de reducción de peso por cada radio
        for (int h = 1; h <= 3; ++h) {
            float dist = hubR + h * (28.0f * scale);
            int hx = cx + static_cast<int>(cosA * dist);
            int hy = cy + static_cast<int>(sinA * dist);
            int hRadius = std::max(2, static_cast<int>((4 + h * 2) * scale));
            drawNeobrutalCircle(hx, hy, hRadius, {18, 20, 24, 255}, 2, 0);
        }
    };

    drawPerforatedSpoke(0.20f);  // Radio inferior derecho
    drawPerforatedSpoke(2.94f);  // Radio inferior izquierdo
    drawPerforatedSpoke(kPi * 0.5f); // Radio vertical inferior
}

void Renderer::renderPlayerView(const Road& road, const Car& car, float raceTime, const SDL_Rect& viewport, bool isSplitScreen, int playerNum, const Car* otherCar, SDL_Color otherColor) {
    SDL_RenderSetViewport(ren_, &viewport);

    // 1. Visión hacia adelante promedio (Look-ahead ponderado de los próximos 14 segmentos)
    // Esto elimina por completo los saltos bruscos cuando el auto pasa de una recta a una curva
    int baseSeg = static_cast<int>(std::floor(car.getTrackZ() / Road::kSegmentLength));
    float avgCurve = 0.0f;
    float totalWeight = 0.0f;
    for (int k = 0; k < 14; ++k) {
        float w = 1.0f / (1.0f + k * 0.45f);
        avgCurve += road.getSegment(baseSeg + k).curve * w;
        totalWeight += w;
    }
    avgCurve /= totalWeight;

    // Amortiguación elástica de alta frecuencia (suavizado cinematográfico)
    smoothedCurve_ += (avgCurve - smoothedCurve_) * 0.075f;

    // Desplazamiento orgánico del fondo (montañas y sol con paralaje suave)
    float speedRatio = car.getSpeedRatio();
    skyParallaxX_ += smoothedCurve_ * speedRatio * 1.85f;

    // Inclinación dinámica de cámara suave en curvas (G-force lateral)
    float targetCamTilt = car.getRoll() * 0.0055f;
    smoothedCameraX_ += (targetCamTilt - smoothedCameraX_) * 0.08f;

    // Horizonte dinámico y cabeceo suave
    float bobOffset = std::sin(car.getSuspensionBob()) * (car.isOffRoad() ? 2.5f : 0.6f);
    float baseHorizon = isSplitScreen ? (viewport.h * 0.38f) : 420.0f;
    float dynamicHorizonY = baseHorizon + car.getPitch() * 0.38f + bobOffset;

    renderSky(car.getX(), skyParallaxX_, dynamicHorizonY, raceTime, viewport.w, viewport.h);
    renderRoad(road, car, dynamicHorizonY, smoothedCameraX_, raceTime, viewport.w, viewport.h, otherCar, otherColor);
    renderSpeedEffects(car, dynamicHorizonY, raceTime, viewport.w, viewport.h);
    renderCockpit(car, road, raceTime, viewport.w, viewport.h, isSplitScreen, otherCar, otherColor);
}

void Renderer::renderSplitDivider(float p1Z, float p2Z, float trackLength) {
    // Resetear viewport a pantalla completa 1920x1080 para la barra central
    SDL_RenderSetViewport(ren_, nullptr);

    int divY = 534;
    int divH = 12;

    // Sombra dura negra del divisor neobrutalista
    drawNeobrutalRect(0, divY, kScreenWidth, divH, {18, 18, 18, 255}, 0, 0);

    // Barra de color contrastada con patrón de cuadros de bandera de llegada en el centro
    SDL_Color stripeCol = {250, 204, 21, 255}; // Amarillo Cyberpunk
    SDL_Rect barR = {0, divY + 3, kScreenWidth, 6};
    SDL_SetRenderDrawColor(ren_, stripeCol.r, stripeCol.g, stripeCol.b, stripeCol.a);
    SDL_RenderFillRect(ren_, &barR);

    // Placa central de diferencia métrica entre ambos pilotos
    float distDiff = p1Z - p2Z;
    while (distDiff > trackLength * 0.5f) distDiff -= trackLength;
    while (distDiff < -trackLength * 0.5f) distDiff += trackLength;

    char gapBuf[48];
    if (std::abs(distDiff) < 1.0f) {
        std::snprintf(gapBuf, sizeof(gapBuf), "TIED - WHEEL TO WHEEL");
    } else if (distDiff > 0.0f) {
        std::snprintf(gapBuf, sizeof(gapBuf), "P1 LEADS +%.0fm", distDiff);
    } else {
        std::snprintf(gapBuf, sizeof(gapBuf), "P2 LEADS +%.0fm", -distDiff);
    }

    int badgeW = 320;
    int badgeH = 26;
    int badgeX = (kScreenWidth / 2) - (badgeW / 2);
    int badgeY = 540 - badgeH / 2;

    drawNeobrutalRect(badgeX, badgeY, badgeW, badgeH, {15, 23, 42, 255}, 2, 2);
    drawText(fontSmall_, gapBuf, kScreenWidth / 2, badgeY + 4, {254, 240, 138, 255}, true);
}

void Renderer::renderGame(const Road& road, const Car& car, float raceTime, const Car* otherCar, SDL_Color otherCarColor) {
    SDL_SetRenderDrawColor(ren_, 15, 23, 42, 255);
    SDL_RenderClear(ren_);

    if (otherCar != nullptr) {
        // MODO PANTALLA DIVIDIDA HORIZONTAL 2P (JUGADOR 1 ARRIBA, JUGADOR 2 ABAJO)
        SDL_Rect viewTop = {0, 0, kScreenWidth, 540};
        renderPlayerView(road, car, raceTime, viewTop, true, 1, otherCar, otherCarColor);

        SDL_Rect viewBottom = {0, 540, kScreenWidth, 540};
        renderPlayerView(road, *otherCar, raceTime, viewBottom, true, 2, &car, {6, 182, 212, 255});

        renderSplitDivider(car.getTrackZ(), otherCar->getTrackZ(), road.getTrackLength());
    } else {
        // MODO PANTALLA COMPLETA 1P
        SDL_Rect fullView = {0, 0, kScreenWidth, kScreenHeight};
        renderPlayerView(road, car, raceTime, fullView, false, 1, nullptr);
    }
}
