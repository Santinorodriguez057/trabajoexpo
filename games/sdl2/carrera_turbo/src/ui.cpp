#include "ui.h"
#include <cstdio>
#include <algorithm>
#include <vector>

UserInterface::UserInterface(Renderer& renderer, TTF_Font* fontTitle, TTF_Font* fontHud, TTF_Font* fontSmall, DrivingConfig& config)
    : renderer_(renderer), fontTitle_(fontTitle), fontHud_(fontHud), fontSmall_(fontSmall), config_(config) {
}

void UserInterface::renderCalibration(const InputState& input, int playerNum, bool isSplit) {
    int modalW = isSplit ? 980 : 1200;
    int modalH = isSplit ? 460 : 740;
    int modalX = 960 - modalW / 2;
    int modalY = isSplit ? (playerNum == 1 ? 40 : 580) : (540 - modalH / 2);

    renderer_.drawNeobrutalRect(modalX, modalY, modalW, modalH, {255, 255, 255, 255}, 5, 8);

    // Cabecera Neobrutalista
    SDL_Color headerCol = (playerNum == 1) ? SDL_Color{56, 189, 248, 255} : SDL_Color{244, 63, 94, 255};
    renderer_.drawNeobrutalRect(modalX + 30, modalY - 24, modalW - 60, isSplit ? 54 : 75, headerCol, 4, 6);
    std::string title = isSplit ? ("CALIBRACIÓN VOLANTE - JUGADOR " + std::to_string(playerNum)) : "CALIBRACIÓN DEL VOLANTE DUALSHOCK";
    renderer_.drawText(isSplit ? fontHud_ : fontTitle_, title, 960, modalY - (isSplit ? 18 : 22), {18, 18, 18, 255}, true);

    // Instrucciones
    if (!isSplit) {
        renderer_.drawText(fontHud_, "Mano Izquierda: Sostén el mando | [L2] Acelerar • [L1] Frenar", 960, modalY + 80, {24, 24, 27, 255}, true);
        renderer_.drawText(fontSmall_, "Mano Derecha: Palanca de Cambios secuencial • Inclina para girar.", 960, modalY + 130, {75, 85, 99, 255}, true);
    }

    // Cuadro interactivo con volante girando en vivo
    int previewBoxW = isSplit ? 340 : 440;
    int previewBoxH = isSplit ? 180 : 260;
    int previewBoxX = 960 - previewBoxW / 2;
    int previewBoxY = isSplit ? modalY + 48 : modalY + 180;
    renderer_.drawNeobrutalRect(previewBoxX, previewBoxY, previewBoxW, previewBoxH, {30, 41, 59, 255}, 4, 6);

    // Volante miniatura interactivo
    int pWheelRadius = isSplit ? 52 : 75;
    int pWheelCenterY = previewBoxY + previewBoxH / 2 + 10;
    renderer_.drawNeobrutalCircle(960, pWheelCenterY, pWheelRadius, {18, 18, 18, 255}, 4, 3);
    renderer_.drawNeobrutalCircle(960, pWheelCenterY, pWheelRadius - 16, {30, 41, 59, 255}, 2, 0);
    renderer_.drawNeobrutalCircle(960, pWheelCenterY, 18, {244, 63, 94, 255}, 2, 0);

    // Franja indicadora superior rotada
    float rad = input.physicalAngleDeg * 0.0174532925f - 1.57079f;
    int tapeX = 960 + static_cast<int>(std::cos(rad) * (pWheelRadius - 8));
    int tapeY = pWheelCenterY + static_cast<int>(std::sin(rad) * (pWheelRadius - 8));
    renderer_.drawNeobrutalCircle(tapeX, tapeY, 8, {250, 204, 21, 255}, 2, 0);

    // Telemetría en vivo
    char angBuf[64];
    std::snprintf(angBuf, sizeof(angBuf), "ÁNGULO DEL VOLANTE: %+.1f°", input.physicalAngleDeg);
    renderer_.drawText(fontSmall_, angBuf, 960, previewBoxY + 12, {250, 204, 21, 255}, true);

    // Botón de confirmación
    int btnY = isSplit ? (modalY + modalH - 50) : (modalY + modalH - 55);
    renderer_.drawNeobrutalRect(960 - 240, btnY, 480, isSplit ? 45 : 65, {34, 197, 94, 255}, 4, 6);
    renderer_.drawText(isSplit ? fontSmall_ : fontHud_, "PRESIONA [X] O [ENTER] PARA COMENZAR", 960, btnY + (isSplit ? 10 : 15), {18, 18, 18, 255}, true);
}

void UserInterface::renderHud(const Car& car, float timeRemaining, int checkpointNum, float bonusBannerTimer, bool isSplit, int playerNum, float rivalGap) {
    if (!isSplit) {
        // 1. Contador de Tiempo Restante (Caja Neobrutalista Amarilla)
        int timeW = 280;
        int timeH = 75;
        int timeX = 60;
        int timeY = 40;
        renderer_.drawNeobrutalRect(timeX, timeY, timeW, timeH, {250, 204, 21, 255}, 4, 6);

        char tBuf[32];
        std::snprintf(tBuf, sizeof(tBuf), "TIEMPO: %.1fs", std::max(0.0f, timeRemaining));
        renderer_.drawText(fontHud_, tBuf, timeX + timeW / 2, timeY + 16, {18, 18, 18, 255}, true);

        // 2. Distancia y Checkpoints (Caja Neobrutalista Cian)
        int cpW = 320;
        int cpH = 75;
        int cpX = 1920 - cpW - 60;
        int cpY = 40;
        renderer_.drawNeobrutalRect(cpX, cpY, cpW, cpH, {56, 189, 248, 255}, 4, 6);

        std::string cpStr = "CHECKPOINT: " + std::to_string(checkpointNum);
        renderer_.drawText(fontHud_, cpStr, cpX + cpW / 2, cpY + 16, {18, 18, 18, 255}, true);

        // 3. Banner animado al cruzar Checkpoint
        if (bonusBannerTimer > 0.0f) {
            int bannerW = 600;
            int bannerH = 90;
            int bannerX = 960 - bannerW / 2;
            int bannerY = 160;
            renderer_.drawNeobrutalRect(bannerX, bannerY, bannerW, bannerH, {34, 197, 94, 255}, 5, 8);
            renderer_.drawText(fontTitle_, "¡CHECKPOINT! +25s", 960, bannerY + 14, {18, 18, 18, 255}, true);
        }
    } else {
        // MODO PANTALLA DIVIDIDA: HUD compacto dentro del viewport de cada jugador
        int baseHudY = (playerNum == 1) ? 16 : (540 + 16);

        // Tarjeta Jugador y Tiempo (Izquierda)
        int timeW = 260;
        int timeH = 50;
        SDL_Color pCol = (playerNum == 1) ? SDL_Color{56, 189, 248, 255} : SDL_Color{244, 63, 94, 255};
        renderer_.drawNeobrutalRect(40, baseHudY, timeW, timeH, pCol, 3, 4);

        char tBuf[48];
        std::snprintf(tBuf, sizeof(tBuf), "P%d | %.1fs", playerNum, std::max(0.0f, timeRemaining));
        renderer_.drawText(fontHud_, tBuf, 40 + timeW / 2, baseHudY + 8, {18, 18, 18, 255}, true);

        // Checkpoint y Diferencia con rival (Derecha)
        int cpW = 280;
        int cpH = 50;
        int cpX = 1920 - cpW - 40;
        renderer_.drawNeobrutalRect(cpX, baseHudY, cpW, cpH, {250, 204, 21, 255}, 3, 4);

        char gapBuf[48];
        if (std::abs(rivalGap) < 1.0f) {
            std::snprintf(gapBuf, sizeof(gapBuf), "CP %d • EMPATE", checkpointNum);
        } else if (rivalGap > 0.0f) {
            std::snprintf(gapBuf, sizeof(gapBuf), "CP %d • +%.0fm", checkpointNum, rivalGap);
        } else {
            std::snprintf(gapBuf, sizeof(gapBuf), "CP %d • %.0fm", checkpointNum, rivalGap);
        }
        renderer_.drawText(fontHud_, gapBuf, cpX + cpW / 2, baseHudY + 8, {18, 18, 18, 255}, true);

        // Banner animado de Checkpoint compacto
        if (bonusBannerTimer > 0.0f) {
            int bW = 440;
            int bH = 55;
            int bannerY = (playerNum == 1) ? 75 : (540 + 75);
            renderer_.drawNeobrutalRect(960 - bW / 2, bannerY, bW, bH, {34, 197, 94, 255}, 3, 4);
            renderer_.drawText(fontHud_, "¡CHECKPOINT! +25s", 960, bannerY + 10, {18, 18, 18, 255}, true);
        }
    }
}

void UserInterface::renderTimeAttackHud(const Car& car, const Road& road, int currentLap, int totalLaps, float currentLapTime, float bestLapTime, float lastLapTime, int currentSector, float sectorDelta, float sectorBannerTimer) {
    // =========================================================================
    // HUD NEOBRUTALISTA DE VUELTA RÁPIDA / TIME ATTACK (F1 / Gran Turismo Style)
    // =========================================================================

    // 1. Contador de Vuelta (Lap 1 / 3) - Esquina Superior Izquierda
    int lapBoxW = 220;
    int lapBoxH = 70;
    int lapBoxX = 50;
    int lapBoxY = 35;
    renderer_.drawNeobrutalRect(lapBoxX, lapBoxY, lapBoxW, lapBoxH, {250, 204, 21, 255}, 4, 6);
    char lapBuf[32];
    std::snprintf(lapBuf, sizeof(lapBuf), "LAP %d / %d", std::min(currentLap, totalLaps), totalLaps);
    renderer_.drawText(fontTitle_, lapBuf, lapBoxX + lapBoxW / 2, lapBoxY + 12, {18, 18, 18, 255}, true);

    // 2. Cronómetro de Vuelta Actual (Centro Superior)
    int timeBoxW = 340;
    int timeBoxH = 80;
    int timeBoxX = 960 - timeBoxW / 2;
    int timeBoxY = 30;
    renderer_.drawNeobrutalRect(timeBoxX, timeBoxY, timeBoxW, timeBoxH, {255, 255, 255, 255}, 4, 6);

    int mins = static_cast<int>(currentLapTime) / 60;
    float secs = currentLapTime - mins * 60.0f;
    char timeStr[48];
    std::snprintf(timeStr, sizeof(timeStr), "%02d:%05.2f", mins, secs);
    renderer_.drawText(fontTitle_, timeStr, 960, timeBoxY + 14, {24, 24, 27, 255}, true);

    // Etiqueta superior sobre la caja
    renderer_.drawNeobrutalRect(960 - 70, timeBoxY - 14, 140, 26, {34, 197, 94, 255}, 2, 0);
    renderer_.drawText(fontSmall_, "TIEMPO ACTUAL", 960, timeBoxY - 10, {18, 18, 18, 255}, true);

    // 3. Tarjeta de Mejor Vuelta y Última Vuelta (Esquina Superior Derecha)
    int bestBoxW = 320;
    int bestBoxH = 84;
    int bestBoxX = 1920 - bestBoxW - 50;
    int bestBoxY = 35;
    renderer_.drawNeobrutalRect(bestBoxX, bestBoxY, bestBoxW, bestBoxH, {18, 20, 26, 255}, 4, 6);

    char bestBuf[48];
    if (bestLapTime < 9990.0f) {
        int bMins = static_cast<int>(bestLapTime) / 60;
        float bSecs = bestLapTime - bMins * 60.0f;
        std::snprintf(bestBuf, sizeof(bestBuf), "BEST: %02d:%05.2f", bMins, bSecs);
    } else {
        std::snprintf(bestBuf, sizeof(bestBuf), "BEST: --:--.--");
    }
    renderer_.drawText(fontHud_, bestBuf, bestBoxX + bestBoxW / 2, bestBoxY + 10, {168, 85, 247, 255}, true);

    char lastBuf[48];
    if (lastLapTime > 0.0f) {
        int lMins = static_cast<int>(lastLapTime) / 60;
        float lSecs = lastLapTime - lMins * 60.0f;
        std::snprintf(lastBuf, sizeof(lastBuf), "LAST: %02d:%05.2f", lMins, lSecs);
    } else {
        std::snprintf(lastBuf, sizeof(lastBuf), "LAST: --:--.--");
    }
    renderer_.drawText(fontSmall_, lastBuf, bestBoxX + bestBoxW / 2, bestBoxY + 50, {226, 232, 240, 255}, true);

    // 4. Banner animado al cruzar Sector con Delta (+/- en verde o rojo)
    if (sectorBannerTimer > 0.0f) {
        int bW = 540;
        int bH = 75;
        int bannerX = 960 - bW / 2;
        int bannerY = 125;

        SDL_Color bannerCol = (sectorDelta <= 0.0f) ? SDL_Color{34, 197, 94, 255} : SDL_Color{239, 68, 68, 255};
        renderer_.drawNeobrutalRect(bannerX, bannerY, bW, bH, bannerCol, 4, 6);

        char deltaBuf[64];
        if (bestLapTime >= 9990.0f) {
            std::snprintf(deltaBuf, sizeof(deltaBuf), "SECTOR %d COMPLETADO", currentSector);
        } else if (sectorDelta <= 0.0f) {
            std::snprintf(deltaBuf, sizeof(deltaBuf), "SECTOR %d  •  %.2fs", currentSector, sectorDelta);
        } else {
            std::snprintf(deltaBuf, sizeof(deltaBuf), "SECTOR %d  •  +%.2fs", currentSector, sectorDelta);
        }
        renderer_.drawText(fontTitle_, deltaBuf, 960, bannerY + 12, {18, 18, 18, 255}, true);
    }

    // 4.1 Aviso dinámico de curva peligrosa / frenada necesaria (Braking Indicator)
    // Analiza la curvatura que viene en los próximos 15 a 35 segmentos (~180m a 420m adelante)
    int currentSegIdx = static_cast<int>(std::floor(car.getTrackZ() / Road::kSegmentLength));
    float maxAheadCurve = 0.0f;
    for (int k = 8; k < 28; ++k) {
        float cMag = std::abs(road.getSegment(currentSegIdx + k).curve);
        if (cMag > maxAheadCurve) maxAheadCurve = cMag;
    }

    // Velocidad segura recomendada según el radio de la curva venidera
    float safeSpeed = 260.0f;
    int suggestedGear = 6;
    if (maxAheadCurve > 4.5f) {
        safeSpeed = 125.0f; // Horquillas o chicanes lentas
        suggestedGear = 2;
    } else if (maxAheadCurve > 3.6f) {
        safeSpeed = 160.0f; // Curvas técnicas medias
        suggestedGear = 3;
    } else if (maxAheadCurve > 2.6f) {
        safeSpeed = 195.0f;
        suggestedGear = 4;
    }

    // Si el auto va más rápido de lo seguro para la curva que se avecina, activar aviso
    if (car.getSpeed() > safeSpeed + 15.0f && maxAheadCurve >= 3.2f) {
        int warnW = 480;
        int warnH = 75;
        int warnX = 960 - warnW / 2;
        int warnY = (sectorBannerTimer > 0.0f) ? 215 : 125;

        // Pulso de advertencia rojo neobrutalista
        renderer_.drawNeobrutalRect(warnX, warnY, warnW, warnH, {239, 68, 68, 255}, 4, 6);

        char warnBuf[64];
        std::snprintf(warnBuf, sizeof(warnBuf), "¡ FRENA !  REDUCE A %dª", suggestedGear);
        renderer_.drawText(fontTitle_, warnBuf, 960, warnY + 12, {255, 255, 255, 255}, true);
    }

    // 5. Mini Mapa del Circuito Técnico (Esquina Derecha de la Pantalla)
    int mapW = 270;
    int mapH = 290;
    int mapX = 1920 - mapW - 40;
    int mapY = 140;
    renderMinimap(road, car.getTrackZ(), currentSector, mapX, mapY, mapW, mapH);
}

void UserInterface::renderMinimap(const Road& road, float playerZ, int currentSector, int mapX, int mapY, int mapW, int mapH) {
    const auto& segments = road.getSegments();
    if (segments.empty()) return;

    // Caja Neobrutalista del Mini Mapa
    renderer_.drawNeobrutalRect(mapX, mapY, mapW, mapH, {24, 26, 32, 240}, 4, 6);

    // Encabezado de Sectores arriba del mapa
    renderer_.drawNeobrutalRect(mapX + 12, mapY + 10, mapW - 24, 32, {15, 23, 42, 255}, 2, 0);

    // Indicadores de los 3 sectores (S1, S2, S3)
    int secBtnW = (mapW - 32) / 3;
    for (int s = 1; s <= 3; ++s) {
        int sx = mapX + 16 + (s - 1) * secBtnW;
        bool isActive = (s == currentSector);
        SDL_Color bgCol = isActive ? SDL_Color{250, 204, 21, 255} : SDL_Color{39, 42, 52, 255};
        SDL_Color textCol = isActive ? SDL_Color{18, 18, 18, 255} : SDL_Color{148, 163, 184, 255};
        renderer_.drawNeobrutalRect(sx, mapY + 13, secBtnW - 4, 26, bgCol, 1, 0);
        char sName[16];
        std::snprintf(sName, sizeof(sName), "S%d", s);
        renderer_.drawText(fontSmall_, sName, sx + (secBtnW - 4) / 2, mapY + 16, textCol, true);
    }

    // Calcular la silueta 2D del circuito trazando la curvatura segmento a segmento
    static std::vector<std::pair<float, float>> track2D;
    static float minX = 0, maxX = 0, minY = 0, maxY = 0;
    static size_t lastSegCount = 0;

    if (track2D.size() != segments.size() || lastSegCount != segments.size()) {
        track2D.clear();
        int nSegs = static_cast<int>(segments.size());
        track2D.reserve(nSegs);

        // Factor de curvatura realista para trazado de circuito (sin bucles ni círculos cerrados)
        const float kAngle = 0.0075f;

        // 2. Integrar posiciones acumuladas
        std::vector<std::pair<float, float>> rawPoints;
        rawPoints.reserve(nSegs + 1);
        rawPoints.push_back({0.0f, 0.0f});

        float curAngle = 0.0f;
        for (const auto& seg : segments) {
            curAngle += seg.curve * kAngle;
            float px = rawPoints.back().first + std::sin(curAngle) * Road::kSegmentLength;
            float py = rawPoints.back().second - std::cos(curAngle) * Road::kSegmentLength;
            rawPoints.push_back({px, py});
        }

        // 3. Error de desplazamiento final (gap entre final de S3 e inicio de S1)
        float endDx = rawPoints.back().first;
        float endDy = rawPoints.back().second;

        minX = maxX = 0.0f;
        minY = maxY = 0.0f;

        // 4. Distribuir el cierre suavemente a lo largo de toda la pista para un bucle continuo perfecto
        for (int i = 0; i < nSegs; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(nSegs);
            float cx = rawPoints[i].first - endDx * t;
            float cy = rawPoints[i].second - endDy * t;
            track2D.push_back({cx, cy});

            if (i == 0) {
                minX = maxX = cx;
                minY = maxY = cy;
            } else {
                if (cx < minX) minX = cx;
                if (cx > maxX) maxX = cx;
                if (cy < minY) minY = cy;
                if (cy > maxY) maxY = cy;
            }
        }
        lastSegCount = segments.size();
    }

    // Escalar la silueta al tamaño del recuadro del mapa
    float trackRangeX = std::max(10.0f, maxX - minX);
    float trackRangeY = std::max(10.0f, maxY - minY);

    int plotAreaX = mapX + 24;
    int plotAreaY = mapY + 54;
    int plotAreaW = mapW - 48;
    int plotAreaH = mapH - 74;

    float scale = std::min(plotAreaW / trackRangeX, plotAreaH / trackRangeY) * 0.90f;
    float centerOffsetX = plotAreaX + (plotAreaW - trackRangeX * scale) * 0.5f - minX * scale;
    float centerOffsetY = plotAreaY + (plotAreaH - trackRangeY * scale) * 0.5f - minY * scale;

    int totalSegs = static_cast<int>(segments.size());
    int s1End = totalSegs / 3;
    int s2End = (totalSegs * 2) / 3;

    // Colores de los 3 sectores: S1 = Celeste, S2 = Amarillo Neón, S3 = Magenta Racing
    const SDL_Color sectorColors[3] = {
        {56, 189, 248, 255},  // S1: Celeste Eléctrico
        {250, 204, 21, 255},  // S2: Amarillo Neón
        {244, 63, 94, 255}    // S3: Magenta Racing
    };

    // Dibujar el trazado limpio y continuo de la pista con sus 3 sectores claramente diferenciados
    const int step = 2; // Trazar cada 2 segmentos para máxima suavidad
    for (int i = 0; i < totalSegs; i += step) {
        int nextI = (i + step) % totalSegs;
        float x1 = centerOffsetX + track2D[i].first * scale;
        float y1 = centerOffsetY + track2D[i].second * scale;
        float x2 = centerOffsetX + track2D[nextI].first * scale;
        float y2 = centerOffsetY + track2D[nextI].second * scale;

        int sec = (i < s1End) ? 0 : ((i < s2End) ? 1 : 2);
        SDL_Color lineColor = sectorColors[sec];
        // Resaltar con mayor grosor y brillo el sector por donde va el auto actualmente
        if (sec + 1 == currentSector) {
            renderer_.renderThickLine(x1, y1, x2, y2, 5.0f, lineColor);
        } else {
            renderer_.renderThickLine(x1, y1, x2, y2, 3.0f, lineColor);
        }
    }

    // Línea de meta sutil representada como pequeña barra perpendicular blanca
    if (track2D.size() > 4) {
        float x0 = centerOffsetX + track2D[0].first * scale;
        float y0 = centerOffsetY + track2D[0].second * scale;
        float x1 = centerOffsetX + track2D[4].first * scale;
        float y1 = centerOffsetY + track2D[4].second * scale;
        float dx = x1 - x0;
        float dy = y1 - y0;
        float len = std::hypot(dx, dy);
        if (len > 0.001f) {
            float nx = -dy / len;
            float ny = dx / len;
            renderer_.renderThickLine(x0 - nx * 6.0f, y0 - ny * 6.0f, x0 + nx * 6.0f, y0 + ny * 6.0f, 3.5f, {255, 255, 255, 255});
        }
    }

    // Indicador dinámico de posición del coche: Flecha / Chevron de competición apuntando en la dirección del viaje
    int pIdx = static_cast<int>(playerZ / Road::kSegmentLength) % totalSegs;
    if (pIdx < 0) pIdx += totalSegs;

    if (pIdx < static_cast<int>(track2D.size())) {
        int nextIdx = (pIdx + 4) % totalSegs;
        float curX = centerOffsetX + track2D[pIdx].first * scale;
        float curY = centerOffsetY + track2D[pIdx].second * scale;
        float dirX = (centerOffsetX + track2D[nextIdx].first * scale) - curX;
        float dirY = (centerOffsetY + track2D[nextIdx].second * scale) - curY;
        float dirLen = std::hypot(dirX, dirY);

        if (dirLen > 0.001f) {
            float fwdX = dirX / dirLen;
            float fwdY = dirY / dirLen;
            float rightX = -fwdY;
            float rightY = fwdX;

            float arrowLen = 9.0f;
            float arrowHalfW = 6.0f;

            // Vértices del triángulo de navegación
            float tipX = curX + fwdX * arrowLen;
            float tipY = curY + fwdY * arrowLen;
            float leftX = curX - fwdX * (arrowLen * 0.5f) - rightX * arrowHalfW;
            float leftY = curY - fwdY * (arrowLen * 0.5f) - rightY * arrowHalfW;
            float rightPosX = curX - fwdX * (arrowLen * 0.5f) + rightX * arrowHalfW;
            float rightPosY = curY - fwdY * (arrowLen * 0.5f) + rightY * arrowHalfW;

            // Borde oscuro neobrutalista
            renderer_.renderThickLine(tipX, tipY, leftX, leftY, 2.5f, {18, 18, 18, 255});
            renderer_.renderThickLine(leftX, leftY, rightPosX, rightPosY, 2.5f, {18, 18, 18, 255});
            renderer_.renderThickLine(rightPosX, rightPosY, tipX, tipY, 2.5f, {18, 18, 18, 255});

            // Relleno rojo competición de alta visibilidad
            SDL_Color redFill = {239, 68, 68, 255};
            renderer_.renderTriangle(tipX, tipY, leftX, leftY, rightPosX, rightPosY, redFill, redFill, redFill);
        }
    }
}

void UserInterface::renderTimeAttackFinish(float bestLapTime, float totalSessionTime, float topSpeed, const std::vector<float>& lapTimes) {
    int modalW = 1080;
    int modalH = 680;
    int modalX = 960 - modalW / 2;
    int modalY = 540 - modalH / 2;

    renderer_.drawNeobrutalRect(modalX, modalY, modalW, modalH, {255, 255, 255, 255}, 6, 12);

    // Cabecera Neobrutalista
    renderer_.drawNeobrutalRect(modalX + 50, modalY - 30, modalW - 100, 75, {250, 204, 21, 255}, 5, 8);
    renderer_.drawText(fontTitle_, "¡SESIÓN DE VUELTA RÁPIDA FINALIZADA!", 960, modalY - 22, {18, 18, 18, 255}, true);

    // Mejor Vuelta destacada en púrpura
    int bestBoxW = 600;
    int bestBoxH = 90;
    int bestBoxY = modalY + 80;
    renderer_.drawNeobrutalRect(960 - bestBoxW / 2, bestBoxY, bestBoxW, bestBoxH, {168, 85, 247, 255}, 4, 6);
    char bestStr[64];
    if (bestLapTime < 9990.0f) {
        int bM = static_cast<int>(bestLapTime) / 60;
        float bS = bestLapTime - bM * 60.0f;
        std::snprintf(bestStr, sizeof(bestStr), "MEJOR VUELTA: %02d:%05.2f", bM, bS);
    } else {
        std::snprintf(bestStr, sizeof(bestStr), "MEJOR VUELTA: --:--.--");
    }
    renderer_.drawText(fontTitle_, bestStr, 960, bestBoxY + 18, {255, 255, 255, 255}, true);

    // Desglose de cada vuelta
    int lapListY = modalY + 200;
    for (size_t i = 0; i < lapTimes.size(); ++i) {
        float lt = lapTimes[i];
        int lm = static_cast<int>(lt) / 60;
        float ls = lt - lm * 60.0f;
        char rowBuf[64];
        bool isBest = (std::abs(lt - bestLapTime) < 0.001f);
        std::snprintf(rowBuf, sizeof(rowBuf), "VUELTA %zu:  %02d:%05.2f %s", i + 1, lm, ls, isBest ? " ★ (RECORD)" : "");
        SDL_Color rowCol = isBest ? SDL_Color{34, 197, 94, 255} : SDL_Color{24, 24, 27, 255};
        renderer_.drawText(fontHud_, rowBuf, 960, lapListY + static_cast<int>(i) * 50, rowCol, true);
    }

    // Estadísticas adicionales
    char spdBuf[64];
    std::snprintf(spdBuf, sizeof(spdBuf), "Velocidad Máxima Alcanzada: %.1f km/h", topSpeed);
    renderer_.drawText(fontHud_, spdBuf, 960, modalY + 390, {75, 85, 99, 255}, true);

    // Rango según tiempo
    std::string rank = (bestLapTime < 42.0f) ? "RANGO: S (PILOTO PROFESIONAL DE GT)" : ((bestLapTime < 48.0f) ? "RANGO: A (RITMO EXCELENTE)" : "RANGO: B (BUEN TIEMPO)");
    renderer_.drawText(fontTitle_, rank, 960, modalY + 460, {244, 63, 94, 255}, true);

    // Botones
    renderer_.drawNeobrutalRect(960 - 260, modalY + 540, 520, 60, {34, 197, 94, 255}, 4, 6);
    renderer_.drawText(fontHud_, "Presiona [X] para Correr Otra Sesión", 960, modalY + 552, {18, 18, 18, 255}, true);

    renderer_.drawText(fontSmall_, "Presiona [O] o [ESC] para Salir al Menú", 960, modalY + 630, {107, 114, 128, 255}, true);
}

void UserInterface::renderFinish(float totalTime, float topSpeed, int checkpointsPassed, bool victory, bool isSplit, int winnerPlayer) {
    int modalW = isSplit ? 1100 : 1000;
    int modalH = isSplit ? 600 : 650;
    int modalX = 960 - modalW / 2;
    int modalY = 540 - modalH / 2;

    SDL_Color headerCol = (isSplit && winnerPlayer == 2) ? SDL_Color{244, 63, 94, 255} : (victory ? SDL_Color{34, 197, 94, 255} : SDL_Color{239, 68, 68, 255});
    renderer_.drawNeobrutalRect(modalX, modalY, modalW, modalH, {255, 255, 255, 255}, 6, 12);

    renderer_.drawNeobrutalRect(modalX + 50, modalY - 30, modalW - 100, 75, headerCol, 5, 8);
    std::string title;
    if (isSplit) {
        if (winnerPlayer == 1) title = "¡JUGADOR 1 GANA EL DUELO!";
        else if (winnerPlayer == 2) title = "¡JUGADOR 2 GANA EL DUELO!";
        else title = "¡CARRERA FINALIZADA (EMPATE)!";
    } else {
        title = victory ? "¡CARRERA COMPLETADA!" : "¡TIEMPO AGOTADO!";
    }
    renderer_.drawText(fontTitle_, title, 960, modalY - 22, {18, 18, 18, 255}, true);

    // Estadísticas
    char timeBuf[64];
    std::snprintf(timeBuf, sizeof(timeBuf), "Tiempo Total: %.2f segundos", totalTime);
    renderer_.drawText(fontHud_, timeBuf, 960, modalY + 95, {24, 24, 27, 255}, true);

    char spdBuf[64];
    std::snprintf(spdBuf, sizeof(spdBuf), "Velocidad Máxima: %.1f km/h", topSpeed);
    renderer_.drawText(fontHud_, spdBuf, 960, modalY + 160, {24, 24, 27, 255}, true);

    std::string cpBuf = "Checkpoints Superados: " + std::to_string(checkpointsPassed);
    renderer_.drawText(fontHud_, cpBuf, 960, modalY + 225, {24, 24, 27, 255}, true);

    // Rango o felicitación
    std::string rank = isSplit ? ("FELICITACIONES AL CAMPEÓN") : (victory ? (totalTime < 95.0f ? "RANGO: S (LEGENDARIO)" : "RANGO: A (PILOTO PRO)") : "RANGO: B");
    renderer_.drawText(fontTitle_, rank, 960, modalY + 315, {244, 63, 94, 255}, true);

    // Botones
    renderer_.drawNeobrutalRect(960 - 260, modalY + 440, 520, 60, {250, 204, 21, 255}, 4, 6);
    renderer_.drawText(fontHud_, "Presiona [X] para Jugar de Nuevo", 960, modalY + 452, {18, 18, 18, 255}, true);

    renderer_.drawText(fontSmall_, "Presiona [O] o [ESC] para Salir al Menú", 960, modalY + 540, {107, 114, 128, 255}, true);
}

void UserInterface::renderPause(int selectedIndex) {
    int modalW = 860;
    int modalH = 580;
    int modalX = 960 - modalW / 2;
    int modalY = 540 - modalH / 2;

    renderer_.drawNeobrutalRect(modalX, modalY, modalW, modalH, {255, 255, 255, 255}, 6, 10);

    renderer_.drawNeobrutalRect(modalX + 40, modalY - 25, modalW - 80, 70, {56, 189, 248, 255}, 5, 8);
    renderer_.drawText(fontTitle_, "PAUSA Y AJUSTES", 960, modalY - 18, {18, 18, 18, 255}, true);

    std::vector<std::string> opts = {
        "Sensibilidad Volante: " + std::to_string(config_.steeringSensitivity).substr(0, 4),
        "Zona Muerta: " + std::to_string(config_.deadzone).substr(0, 4),
        "Suavizado Volante: " + std::to_string(config_.steeringSmoothing).substr(0, 4),
        std::string("Vibración Háptica: ") + (config_.rumbleEnabled ? "ACTIVADA" : "DESACTIVADA"),
        "► REANUDAR CARRERA ◄",
        "SALIR AL MENÚ PRINCIPAL"
    };

    int startY = modalY + 100;
    for (size_t i = 0; i < opts.size(); ++i) {
        bool sel = (static_cast<int>(i) == selectedIndex);
        if (sel) {
            renderer_.drawNeobrutalRect(modalX + 60, startY + static_cast<int>(i * 65) - 8, modalW - 120, 52, {250, 204, 21, 255}, 3, 4);
            renderer_.drawText(fontHud_, opts[i], 960, startY + static_cast<int>(i * 65), {18, 18, 18, 255}, true);
        } else {
            renderer_.drawText(fontHud_, opts[i], 960, startY + static_cast<int>(i * 65), {75, 85, 99, 255}, true);
        }
    }
}
