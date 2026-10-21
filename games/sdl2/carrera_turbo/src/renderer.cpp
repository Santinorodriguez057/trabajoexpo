#include "renderer.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

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
    auto fillCircle = [&](int px, int py, int r, SDL_Color col) {
        SDL_SetRenderDrawColor(ren_, col.r, col.g, col.b, col.a);
        for (int w = 0; w <= r * 2; w++) {
            int dx = r - w;
            int dy = static_cast<int>(std::sqrt(r * r - dx * dx));
            SDL_RenderDrawLine(ren_, px + dx, py - dy, px + dx, py + dy);
        }
    };

    if (shadowOffset > 0) {
        fillCircle(cx + shadowOffset, cy + shadowOffset, radius + borderW, {18, 18, 18, 255});
    }
    if (borderW > 0) {
        fillCircle(cx, cy, radius + borderW, {18, 18, 18, 255});
    }
    fillCircle(cx, cy, radius, fill);
}

void Renderer::drawText(TTF_Font* font, const std::string& text, int x, int y, SDL_Color color, bool center) {
    if (!font || text.empty()) return;

    // Sombra dura neobrutalista del texto
    SDL_Surface* surfShadow = TTF_RenderUTF8_Blended(font, text.c_str(), {18, 18, 18, 255});
    if (surfShadow) {
        SDL_Texture* texS = SDL_CreateTextureFromSurface(ren_, surfShadow);
        SDL_FreeSurface(surfShadow);
        if (texS) {
            int w, h;
            SDL_QueryTexture(texS, nullptr, nullptr, &w, &h);
            SDL_Rect dstS = {center ? x - w / 2 + 4 : x + 4, y + 4, w, h};
            SDL_RenderCopy(ren_, texS, nullptr, &dstS);
            SDL_DestroyTexture(texS);
        }
    }

    // Texto principal
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
    verts[0].tex_coord = {0,0};
    verts[1].position = { static_cast<float>(x2), static_cast<float>(y2) };
    verts[1].color = color;
    verts[1].tex_coord = {0,0};
    verts[2].position = { static_cast<float>(x3), static_cast<float>(y3) };
    verts[2].color = color;
    verts[2].tex_coord = {0,0};
    verts[3].position = { static_cast<float>(x4), static_cast<float>(y4) };
    verts[3].color = color;
    verts[3].tex_coord = {0,0};

    int indices[6] = { 0, 1, 2, 0, 2, 3 };
    SDL_RenderGeometry(ren_, nullptr, verts, 4, indices, 6);
}

void Renderer::renderSky(float carX, float curve) {
    // Cielo neobrutalista en bloques de color saturados
    // Bloque superior: Azul Eléctrico
    drawNeobrutalRect(0, 0, kScreenWidth, 260, {14, 165, 233, 255}, 0, 0);

    // Bloque medio: Magenta / Rosa Neón
    drawNeobrutalRect(0, 260, kScreenWidth, 160, {244, 63, 94, 255}, 0, 0);

    // Sol Neobrutalista masivo en el horizonte
    int sunX = 960 - static_cast<int>(curve * 400.0f);
    drawNeobrutalCircle(sunX, 320, 140, {253, 224, 71, 255}, 6, 8);

    // Cordillera de montañas geométricas (siluetas con bordes negros gruesos)
    for (int x = -200; x < kScreenWidth + 300; x += 180) {
        int mX = x - static_cast<int>(carX * 60.0f);
        int mH = 140 + ((x / 180) % 3) * 50;
        int topY = 420 - mH;

        // Triángulo de montaña
        SDL_Vertex tri[3];
        tri[0].position = { static_cast<float>(mX), 420.0f };
        tri[0].color = {67, 56, 202, 255}; // Añil profundo
        tri[0].tex_coord = {0,0};
        tri[1].position = { static_cast<float>(mX + 110), static_cast<float>(topY) };
        tri[1].color = {99, 102, 241, 255};
        tri[1].tex_coord = {0,0};
        tri[2].position = { static_cast<float>(mX + 220), 420.0f };
        tri[2].color = {67, 56, 202, 255};
        tri[2].tex_coord = {0,0};
        int triIdx[3] = {0, 1, 2};
        SDL_RenderGeometry(ren_, nullptr, tri, 3, triIdx, 3);

        // Borde negro grueso
        SDL_SetRenderDrawColor(ren_, 18, 18, 18, 255);
        for (int b = -2; b <= 2; ++b) {
            SDL_RenderDrawLine(ren_, mX + b, 420, mX + 110 + b, topY);
            SDL_RenderDrawLine(ren_, mX + 110 + b, topY, mX + 220 + b, 420);
        }
    }
}

void Renderer::renderRoad(const Road& road, const Car& car) {
    float playerZ = car.getTrackZ();
    float playerX = car.getX();
    float cameraH = kCameraHeight;

    const RoadSegment& baseSeg = road.findSegment(playerZ);
    int baseIdx = baseSeg.index;

    float maxY = static_cast<float>(kScreenHeight);
    float roadW = Road::kRoadWidth;

    struct ProjectedPoint {
        float screenX = 0.0f;
        float screenY = 0.0f;
        float screenW = 0.0f;
        float scale = 0.0f;
    };

    std::vector<ProjectedPoint> projected(kDrawDistance + 1);

    // 1. Proyección de segmentos hacia adelante
    float curX = 0.0f;
    float ddx = 0.0f;

    for (int n = 0; n < kDrawDistance; ++n) {
        const RoadSegment& seg = road.getSegment(baseIdx + n);
        float segZ = (baseIdx + n) * Road::kSegmentLength;
        float relZ = segZ - playerZ;
        if (relZ <= 1.0f) relZ = 1.0f;

        float scale = (kCameraDepth * kScreenWidth) / relZ;

        curX += ddx;
        ddx += seg.curve;

        float worldX = curX - (playerX * roadW);
        float worldY = seg.worldY - cameraH;

        float sx = (kScreenWidth / 2.0f) + (scale * worldX);
        float sy = (kScreenHeight / 2.0f) - (scale * worldY);
        float sw = scale * roadW;

        projected[n] = {sx, sy, sw, scale};
    }

    // 2. Renderizado de pista (atrás hacia adelante para clipping o de frente atrás)
    // Usamos el clásico painter's algorithm de segmentos adelante hacia la cámara
    for (int n = kDrawDistance - 1; n > 0; --n) {
        const auto& p1 = projected[n - 1];
        const auto& p2 = projected[n];

        if (p2.screenY >= maxY) continue;
        if (p1.screenY <= p2.screenY) continue;

        const RoadSegment& seg = road.getSegment(baseIdx + n - 1);

        // A. Césped lateral (izq y der)
        renderPolygon(0, static_cast<int>(p1.screenY),
                      kScreenWidth, static_cast<int>(p1.screenY),
                      kScreenWidth, static_cast<int>(p2.screenY),
                      0, static_cast<int>(p2.screenY), seg.grassColor);

        // B. Arcén / Rumble strips (pianos rojos y blancos con borde negro)
        float rumbleW1 = p1.screenW * 1.25f;
        float rumbleW2 = p2.screenW * 1.25f;

        renderPolygon(static_cast<int>(p1.screenX - rumbleW1), static_cast<int>(p1.screenY),
                      static_cast<int>(p1.screenX + rumbleW1), static_cast<int>(p1.screenY),
                      static_cast<int>(p2.screenX + rumbleW2), static_cast<int>(p2.screenY),
                      static_cast<int>(p2.screenX - rumbleW2), static_cast<int>(p2.screenY), seg.rumbleColor);

        // Borde negro exterior de los pianos
        SDL_SetRenderDrawColor(ren_, 18, 18, 18, 255);
        SDL_RenderDrawLine(ren_, static_cast<int>(p1.screenX - rumbleW1), static_cast<int>(p1.screenY),
                                 static_cast<int>(p2.screenX - rumbleW2), static_cast<int>(p2.screenY));
        SDL_RenderDrawLine(ren_, static_cast<int>(p1.screenX + rumbleW1), static_cast<int>(p1.screenY),
                                 static_cast<int>(p2.screenX + rumbleW2), static_cast<int>(p2.screenY));

        // C. Asfalto de la carretera
        renderPolygon(static_cast<int>(p1.screenX - p1.screenW), static_cast<int>(p1.screenY),
                      static_cast<int>(p1.screenX + p1.screenW), static_cast<int>(p1.screenY),
                      static_cast<int>(p2.screenX + p2.screenW), static_cast<int>(p2.screenY),
                      static_cast<int>(p2.screenX - p2.screenW), static_cast<int>(p2.screenY), seg.roadColor);

        // Borde negro entre asfalto y pianos
        SDL_RenderDrawLine(ren_, static_cast<int>(p1.screenX - p1.screenW), static_cast<int>(p1.screenY),
                                 static_cast<int>(p2.screenX - p2.screenW), static_cast<int>(p2.screenY));
        SDL_RenderDrawLine(ren_, static_cast<int>(p1.screenX + p1.screenW), static_cast<int>(p1.screenY),
                                 static_cast<int>(p2.screenX + p2.screenW), static_cast<int>(p2.screenY));

        // D. Línea central discontinua
        if (seg.laneColor.r > 100) {
            float laneW1 = p1.screenW * 0.05f;
            float laneW2 = p2.screenW * 0.05f;
            renderPolygon(static_cast<int>(p1.screenX - laneW1), static_cast<int>(p1.screenY),
                          static_cast<int>(p1.screenX + laneW1), static_cast<int>(p1.screenY),
                          static_cast<int>(p2.screenX + laneW2), static_cast<int>(p2.screenY),
                          static_cast<int>(p2.screenX - laneW2), static_cast<int>(p2.screenY), seg.laneColor);
        }

        // E. Props a los costados de este segmento
        for (const auto& prop : seg.props) {
            int propX = static_cast<int>(p1.screenX + prop.offset * p1.screenW);
            renderProp(prop, propX, static_cast<int>(p1.screenY), p1.scale);
        }
    }

    // 3. Renderizar coches de tráfico
    for (const auto& tCar : road.getTraffic()) {
        float relZ = tCar.z - playerZ;
        if (relZ > 2.0f && relZ < kDrawDistance * Road::kSegmentLength) {
            float scale = (kCameraDepth * kScreenWidth) / relZ;
            int segRelIdx = static_cast<int>(relZ / Road::kSegmentLength);
            segRelIdx = std::clamp(segRelIdx, 0, kDrawDistance - 1);
            float baseSx = projected[segRelIdx].screenX;
            float baseSy = projected[segRelIdx].screenY;
            float baseSw = projected[segRelIdx].screenW;

            int carX = static_cast<int>(baseSx + tCar.x * baseSw);
            int carY = static_cast<int>(baseSy);
            renderTrafficCar(tCar, carX, carY, scale);
        }
    }
}

void Renderer::renderProp(const RoadProp& prop, int screenX, int screenY, float scale) {
    if (scale <= 0.001f) return;

    if (prop.type == PropType::PALM_TREE) {
        int trunkW = std::max(6, static_cast<int>(24 * scale * 120.0f));
        int trunkH = std::max(20, static_cast<int>(180 * scale * 120.0f));
        // Tronco neobrutalista
        drawNeobrutalRect(screenX - trunkW / 2, screenY - trunkH, trunkW, trunkH, {180, 83, 9, 255}, 3, 4);

        // Copa de la palmera (tres abanicos verdes con bordes gruesos)
        int crownR = std::max(16, static_cast<int>(70 * scale * 120.0f));
        drawNeobrutalCircle(screenX, screenY - trunkH, crownR, {34, 197, 94, 255}, 4, 4);
        drawNeobrutalCircle(screenX - crownR / 2, screenY - trunkH + 10, crownR * 2 / 3, {74, 222, 128, 255}, 3, 3);
        drawNeobrutalCircle(screenX + crownR / 2, screenY - trunkH + 10, crownR * 2 / 3, {74, 222, 128, 255}, 3, 3);

    } else if (prop.type == PropType::PINE_TREE) {
        int trunkW = std::max(6, static_cast<int>(20 * scale * 120.0f));
        int trunkH = std::max(20, static_cast<int>(160 * scale * 120.0f));
        drawNeobrutalRect(screenX - trunkW / 2, screenY - trunkH / 3, trunkW, trunkH / 3, {120, 53, 15, 255}, 2, 3);

        // Tres triángulos apilados
        int pW = std::max(24, static_cast<int>(90 * scale * 120.0f));
        drawNeobrutalRect(screenX - pW / 2, screenY - trunkH, pW, trunkH * 3 / 4, {22, 101, 52, 255}, 3, 4);

    } else if (prop.type == PropType::BILLBOARD_NEO) {
        int bW = std::max(40, static_cast<int>(180 * scale * 120.0f));
        int bH = std::max(25, static_cast<int>(90 * scale * 120.0f));
        int postH = bH * 3 / 2;

        // Postes
        drawNeobrutalRect(screenX - bW / 3, screenY - postH, 10, postH, {75, 85, 99, 255}, 2, 2);
        drawNeobrutalRect(screenX + bW / 3, screenY - postH, 10, postH, {75, 85, 99, 255}, 2, 2);

        // Cartel Neobrutalista
        drawNeobrutalRect(screenX - bW / 2, screenY - postH - bH, bW, bH, {250, 204, 21, 255}, 4, 6);
        drawText(fontSmall_, "TURBO", screenX, screenY - postH - bH / 2 - 14, {18, 18, 18, 255}, true);

    } else if (prop.type == PropType::CHECKPOINT_ARCH) {
        int archW = static_cast<int>(kScreenWidth * 0.9f * scale * 2.0f);
        if (archW > 1800) archW = 1800;
        int archH = std::max(40, static_cast<int>(120 * scale * 120.0f));

        // Pórtico de meta / checkpoint
        drawNeobrutalRect(screenX - archW / 2, screenY - archH * 3, archW, archH, {244, 63, 94, 255}, 5, 8);
        drawText(fontHud_, "CHECKPOINT", screenX, screenY - archH * 3 + 12, {255, 255, 255, 255}, true);
    }
}

void Renderer::renderTrafficCar(const TrafficCar& car, int screenX, int screenY, float scale) {
    if (scale <= 0.001f) return;

    int carW = std::max(20, static_cast<int>(140 * scale * 120.0f));
    int carH = std::max(12, static_cast<int>(80 * scale * 120.0f));

    // Carrocería deportiva retro neobrutalista
    // Parte inferior (chasis)
    drawNeobrutalRect(screenX - carW / 2, screenY - carH, carW, carH / 2, car.bodyColor, 3, 5);

    // Cabina / Ventana trasera
    int cabW = carW * 3 / 4;
    int cabH = carH / 2;
    drawNeobrutalRect(screenX - cabW / 2, screenY - carH - cabH + 2, cabW, cabH, {30, 41, 59, 255}, 3, 3);

    // Luces de freno rojas
    int lightW = std::max(4, carW / 6);
    drawNeobrutalRect(screenX - carW / 2 + 6, screenY - carH + 4, lightW, carH / 4, {239, 68, 68, 255}, 2, 0);
    drawNeobrutalRect(screenX + carW / 2 - lightW - 6, screenY - carH + 4, lightW, carH / 4, {239, 68, 68, 255}, 2, 0);
}

void Renderer::renderCockpit(const Car& car) {
    // 1. Marco del Parabrisas (visión en primera persona)
    // Pilares laterales izquierdo y derecho
    drawNeobrutalRect(-20, 0, 110, kScreenHeight, {24, 24, 27, 255}, 5, 0);
    drawNeobrutalRect(kScreenWidth - 90, 0, 120, kScreenHeight, {24, 24, 27, 255}, 5, 0);
    // Borde superior
    drawNeobrutalRect(0, -20, kScreenWidth, 90, {24, 24, 27, 255}, 5, 0);

    // 2. Salpicadero / Torpedo del coche (parte inferior de la pantalla)
    int dashY = 820;
    drawNeobrutalRect(0, dashY, kScreenWidth, kScreenHeight - dashY, {39, 39, 42, 255}, 6, 0);

    // Embellecedor deportivo en color Neón (Magenta / Cian)
    drawNeobrutalRect(80, dashY - 14, kScreenWidth - 160, 18, {244, 63, 94, 255}, 3, 4);

    // 3. Cuadro de Instrumentos Central / Digital
    int clusterW = 560;
    int clusterH = 160;
    int clusterX = 960 - clusterW / 2;
    int clusterY = dashY + 40;
    drawNeobrutalRect(clusterX, clusterY, clusterW, clusterH, {18, 18, 18, 255}, 4, 6);

    // Velocímetro digital grande
    std::string spdStr = std::to_string(static_cast<int>(car.getSpeed()));
    drawText(fontTitle_, spdStr, clusterX + 130, clusterY + 25, {250, 204, 21, 255}, true);
    drawText(fontSmall_, "KM/H", clusterX + 130, clusterY + 115, {156, 163, 175, 255}, true);

    // Marcha actual (GEAR)
    std::string gearStr = "G: " + std::to_string(car.getGear());
    drawText(fontHud_, gearStr, clusterX + 280, clusterY + 45, {56, 189, 248, 255}, true);

    // Barra de RPM con línea roja
    int rpmBarX = clusterX + 220;
    int rpmBarY = clusterY + 105;
    int rpmBarW = 300;
    int rpmBarH = 26;
    drawNeobrutalRect(rpmBarX, rpmBarY, rpmBarW, rpmBarH, {30, 41, 59, 255}, 3, 2);

    float rpmFraction = std::clamp((car.getRpm() - 1000.0f) / 6500.0f, 0.0f, 1.0f);
    int fillW = static_cast<int>(rpmFraction * rpmBarW);
    if (fillW > 0) {
        SDL_Color rpmCol = (rpmFraction > 0.85f) ? SDL_Color{239, 68, 68, 255} : SDL_Color{74, 222, 128, 255};
        drawNeobrutalRect(rpmBarX, rpmBarY, fillW, rpmBarH, rpmCol, 0, 0);
    }

    // 4. Volante Físico Interactivo que rota con el DualShock
    renderSteeringWheel(960, 970, car.getWheelVisualAngle());
}

void Renderer::renderSteeringWheel(int cx, int cy, float angleDeg) {
    // Aro exterior del volante (rotado)
    float rad = angleDeg * 0.0174532925f; // Deg to Rad

    int wheelRadius = 175;
    // Aro exterior negro
    drawNeobrutalCircle(cx, cy, wheelRadius, {18, 18, 18, 255}, 6, 8);
    // Vaciado interior para formar el aro
    drawNeobrutalCircle(cx, cy, wheelRadius - 38, {39, 39, 42, 255}, 5, 0);

    // Centro / Bocina
    int hubR = 52;
    drawNeobrutalCircle(cx, cy, hubR, {24, 24, 27, 255}, 4, 4);
    drawNeobrutalCircle(cx, cy, 28, {244, 63, 94, 255}, 3, 0); // Botón central

    // 3 Radios deportivos rotados
    auto drawSpoke = [&](float spokeAngle) {
        float totalA = rad + spokeAngle;
        int sx = cx + static_cast<int>(std::cos(totalA) * (wheelRadius - 20));
        int sy = cy + static_cast<int>(std::sin(totalA) * (wheelRadius - 20));
        SDL_SetRenderDrawColor(ren_, 18, 18, 18, 255);
        for (int b = -6; b <= 6; ++b) {
            SDL_RenderDrawLine(ren_, cx, cy + b, sx, sy + b);
        }
    };

    drawSpoke(0.0f);                  // Radio derecho
    drawSpoke(3.14159f);              // Radio izquierdo
    drawSpoke(1.57079f);              // Radio inferior hacia abajo

    // Franja amarilla de centrado superior del volante estilo competición
    float topA = rad - 1.57079f;
    int tapeX = cx + static_cast<int>(std::cos(topA) * (wheelRadius - 18));
    int tapeY = cy + static_cast<int>(std::sin(topA) * (wheelRadius - 18));
    drawNeobrutalCircle(tapeX, tapeY, 14, {250, 204, 21, 255}, 3, 2);
}

void Renderer::renderGame(const Road& road, const Car& car, float raceTime) {
    SDL_SetRenderDrawColor(ren_, 15, 23, 42, 255);
    SDL_RenderClear(ren_);

    renderSky(car.getX(), road.findSegment(car.getTrackZ()).curve);
    renderRoad(road, car);
    renderCockpit(car);
}
