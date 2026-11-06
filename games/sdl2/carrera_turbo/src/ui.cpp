#include "ui.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>

UserInterface::UserInterface(Renderer& renderer, TTF_Font* fontTitle, TTF_Font* fontHud, TTF_Font* fontSmall, DrivingConfig& config)
    : renderer_(renderer), fontTitle_(fontTitle), fontHud_(fontHud), fontSmall_(fontSmall), config_(config) {
    // Carga de la imagen del mando DualShock 4 transparente (sin fondo)
    const char* possiblePaths[] = {
        "assets/dualshock4.bmp",
        "../assets/dualshock4.bmp",
        "../../assets/dualshock4.bmp",
        "/home/santino/proyectos/trabajo expo/assets/dualshock4.bmp"
    };
    for (const char* p : possiblePaths) {
        SDL_Surface* surf = SDL_LoadBMP(p);
        if (surf) {
            controllerTex_ = SDL_CreateTextureFromSurface(renderer_.getRenderer(), surf);
            SDL_FreeSurface(surf);
            if (controllerTex_) {
                SDL_SetTextureBlendMode(controllerTex_, SDL_BLENDMODE_BLEND);
                break;
            }
        }
    }

    // Carga de la imagen de marca de agua itf
    const char* itfPaths[] = {
        "assets/itf_logo.bmp",
        "../assets/itf_logo.bmp",
        "../../assets/itf_logo.bmp",
        "/home/santino/proyectos/trabajo expo/assets/itf_logo.bmp"
    };
    for (const char* p : itfPaths) {
        SDL_Surface* surf = SDL_LoadBMP(p);
        if (surf) {
            watermarkTex_ = SDL_CreateTextureFromSurface(renderer_.getRenderer(), surf);
            SDL_FreeSurface(surf);
            if (watermarkTex_) {
                SDL_SetTextureBlendMode(watermarkTex_, SDL_BLENDMODE_BLEND);
                SDL_SetTextureAlphaMod(watermarkTex_, 85);
                break;
            }
        }
    }
}

UserInterface::~UserInterface() {
    if (controllerTex_) {
        SDL_DestroyTexture(controllerTex_);
        controllerTex_ = nullptr;
    }
    if (watermarkTex_) {
        SDL_DestroyTexture(watermarkTex_);
        watermarkTex_ = nullptr;
    }
}

void UserInterface::renderDualShockDiagram(int cx, int cy, float tiltAngleDeg, bool isSplit) {
    float scale = isSplit ? 0.20f : 0.26f;

    // Si la imagen del DualShock 4 está cargada, la renderizamos con rotación analógica fluida en vivo
    if (controllerTex_) {
        int origW = 986;
        int origH = 680;
        int dstW = static_cast<int>(origW * scale);
        int dstH = static_cast<int>(origH * scale);

        SDL_Rect dstRect = {
            cx - dstW / 2,
            cy - dstH / 2,
            dstW,
            dstH
        };

        // Renderizado del mando rotando de lado a lado con su ángulo real
        SDL_RenderCopyEx(renderer_.getRenderer(), controllerTex_, nullptr, &dstRect,
                         static_cast<double>(tiltAngleDeg), nullptr, SDL_FLIP_NONE);
    }
}

void UserInterface::renderCalibration(const InputState& input, int playerNum, bool isSplit, float holdProgress, bool isHolding) {
    int modalW = isSplit ? 1040 : 1540;
    int modalH = isSplit ? 480 : 810;
    int modalX = 960 - modalW / 2;
    int modalY = isSplit ? (playerNum == 1 ? 30 : 560) : (540 - modalH / 2);

    renderer_.drawNeobrutalRect(modalX, modalY, modalW, modalH, {255, 255, 255, 255}, 5, 8);

    // Cabecera Neobrutalista - Ajustada a todo el ancho del modal para que ningún texto se desborde
    SDL_Color headerCol = isHolding ? SDL_Color{250, 204, 21, 255} : ((playerNum == 1) ? SDL_Color{56, 189, 248, 255} : SDL_Color{244, 63, 94, 255});
    int headerW = modalW - 40;
    int headerH = isSplit ? 54 : 72;
    renderer_.drawNeobrutalRect(960 - headerW / 2, modalY - 26, headerW, headerH, headerCol, 4, 6);
    std::string title = isHolding ? "CALIBRANDO CENTRO DEL VOLANTE... MANTENLO FIRME" : "CALIBRACIÓN INICIAL: SUJETA EL MANDO COMO UN VOLANTE";
    renderer_.drawText(isSplit ? fontSmall_ : fontHud_, title, 960, modalY - (isSplit ? 16 : 14), {18, 18, 18, 255}, true);

    // Instrucciones
    if (!isSplit) {
        renderer_.drawText(fontHud_, "1. Coloca el mando derecho y centrado como si fuera un volante real.", 960, modalY + 62, {24, 24, 27, 255}, true);
        renderer_.drawText(fontSmall_, "2. Manten presionado [X] o [ENTER] para guardar el centro. O presiona [TRIÁNGULO] para recalibrar.", 960, modalY + 102, {75, 85, 99, 255}, true);
    }

    // Panel Doble Lado a Lado (Modelo de Mando 3D + Volante Virtual Interactivo)
    int boxW = isSplit ? 480 : 640;
    int boxH = isSplit ? 230 : 340;
    int boxY = isSplit ? (modalY + 48) : (modalY + 142);

    // 1. CUADRO IZQUIERDO: MODELO DUALSHOCK EXPLICATIVO
    int ctrlBoxX = 960 - boxW - (isSplit ? 15 : 25);
    renderer_.drawNeobrutalRect(ctrlBoxX, boxY, boxW, boxH, {245, 247, 250, 255}, 4, 6);
    // Título de sección
    renderer_.drawNeobrutalRect(ctrlBoxX + 20, boxY - 14, boxW - 40, 28, {56, 189, 248, 255}, 2, 0);
    renderer_.drawText(fontSmall_, "POSTURA DEL VOLANTE (RECTO AL FRENTE)", ctrlBoxX + boxW / 2, boxY - 10, {18, 18, 18, 255}, true);

    // Diagrama animado de DualShock con inclinación en vivo
    int ctrlCenterX = ctrlBoxX + boxW / 2;
    int ctrlCenterY = boxY + boxH / 2 + (isSplit ? 8 : 12);
    renderDualShockDiagram(ctrlCenterX, ctrlCenterY, input.physicalAngleDeg, isSplit);

    // Leyenda inferior explicativa del movimiento lateral
    renderer_.drawText(fontSmall_, "« GIRO IZQUIERDA        •        GIRO DERECHA »", ctrlCenterX, boxY + boxH - (isSplit ? 22 : 30), {100, 116, 139, 255}, true);

    // 2. CUADRO DERECHO: VOLANTE TELEMETRÍA EN VIVO
    int wheelBoxX = 960 + (isSplit ? 15 : 25);
    renderer_.drawNeobrutalRect(wheelBoxX, boxY, boxW, boxH, {30, 41, 59, 255}, 4, 6);
    renderer_.drawNeobrutalRect(wheelBoxX + 20, boxY - 14, boxW - 40, 28, {250, 204, 21, 255}, 2, 0);
    renderer_.drawText(fontSmall_, "NIVEL DE CENTRADO EN VIVO", wheelBoxX + boxW / 2, boxY - 10, {18, 18, 18, 255}, true);

    int wheelCenterX = wheelBoxX + boxW / 2;
    int pWheelRadius = isSplit ? 60 : 88;
    int pWheelCenterY = boxY + boxH / 2 + 10;
    renderer_.drawNeobrutalCircle(wheelCenterX, pWheelCenterY, pWheelRadius, {18, 18, 18, 255}, 4, 3);
    renderer_.drawNeobrutalCircle(wheelCenterX, pWheelCenterY, pWheelRadius - (isSplit ? 14 : 20), {30, 41, 59, 255}, 2, 0);

    // Marca de centro fija de referencia a las 12 en punto (arriba / 0°)
    renderer_.drawNeobrutalRect(wheelCenterX - 3, pWheelCenterY - pWheelRadius - 10, 6, 12, {239, 68, 68, 255}, 1, 0);

    // Radios del volante rotados con el ángulo físico
    float rad = input.physicalAngleDeg * 0.0174532925f - 1.57079f;
    float steerRot = input.physicalAngleDeg * 0.0174532925f;
    float spokeAngles[3] = {3.14159f + steerRot, 0.0f + steerRot, 1.57079f + steerRot};
    for (int s = 0; s < 3; ++s) {
        int sx = wheelCenterX + static_cast<int>(std::cos(spokeAngles[s]) * (pWheelRadius - 10));
        int sy = pWheelCenterY + static_cast<int>(std::sin(spokeAngles[s]) * (pWheelRadius - 10));
        renderer_.renderThickLine(wheelCenterX, pWheelCenterY, sx, sy, isSplit ? 4.0f : 6.0f, {205, 210, 218, 255});
    }

    // Núcleo central
    renderer_.drawNeobrutalCircle(wheelCenterX, pWheelCenterY, isSplit ? 16 : 24, {244, 63, 94, 255}, 2, 0);

    // Franja indicadora superior rotada en el aro
    int tapeX = wheelCenterX + static_cast<int>(std::cos(rad) * (pWheelRadius - 8));
    int tapeY = pWheelCenterY + static_cast<int>(std::sin(rad) * (pWheelRadius - 8));
    renderer_.drawNeobrutalCircle(tapeX, tapeY, isSplit ? 7 : 9, {250, 204, 21, 255}, 2, 0);

    // Telemetría de ángulo y estado
    char angBuf[64];
    bool isCentered = std::abs(input.physicalAngleDeg) < 3.5f;
    SDL_Color angCol = isCentered ? SDL_Color{34, 197, 94, 255} : (std::abs(input.physicalAngleDeg) < 12.0f ? SDL_Color{250, 204, 21, 255} : SDL_Color{239, 68, 68, 255});
    std::snprintf(angBuf, sizeof(angBuf), "%s (ÁNGULO: %+.1f°)", isCentered ? "¡PERFECTO! CENTRADO" : (input.physicalAngleDeg < 0.0f ? "GIRA A LA DERECHA ->" : "<- GIRA A LA IZQUIERDA"), input.physicalAngleDeg);
    renderer_.drawText(fontSmall_, angBuf, wheelCenterX, boxY + boxH - (isSplit ? 22 : 30), angCol, true);

    // 3. Botón de Recalibración Rápida con Triángulo [▲]
    int recalBtnW = 620;
    int recalBtnH = 46;
    int recalBtnY = modalY + modalH - 146;
    renderer_.drawNeobrutalRect(960 - recalBtnW / 2, recalBtnY, recalBtnW, recalBtnH, {250, 204, 21, 255}, 3, 4);
    renderer_.drawText(fontSmall_, "Presiona [▲ / TRIÁNGULO] para Recalibrar Centro", 960, recalBtnY + 12, {18, 18, 18, 255}, true);

    // 4. Barra de Progreso y Confirmación de Calibración
    int barW = isSplit ? 680 : 920;
    int barH = isSplit ? 46 : 56;
    int barY = isSplit ? (modalY + modalH - 52) : (modalY + modalH - 80);
    int barX = 960 - barW / 2;

    renderer_.drawNeobrutalRect(barX, barY, barW, barH, {241, 245, 249, 255}, 4, 6);

    if (holdProgress > 0.0f) {
        int fillW = static_cast<int>((barW - 8) * std::clamp(holdProgress, 0.0f, 1.0f));
        SDL_Color fillCol = (holdProgress >= 1.0f) ? SDL_Color{34, 197, 94, 255} : SDL_Color{56, 189, 248, 255};
        renderer_.drawNeobrutalRect(barX + 4, barY + 4, fillW, barH - 8, fillCol, 0, 0);
    }

    // Texto de acción dentro del botón/barra
    if (holdProgress >= 1.0f) {
        renderer_.drawText(isSplit ? fontSmall_ : fontHud_, "¡VOLANTE CALIBRADO CON ÉXITO!", 960, barY + (isSplit ? 10 : 12), {18, 18, 18, 255}, true);
    } else if (isHolding) {
        char holdBuf[64];
        std::snprintf(holdBuf, sizeof(holdBuf), "MANTÉN FIRME... %d%%", static_cast<int>(holdProgress * 100.0f));
        renderer_.drawText(isSplit ? fontSmall_ : fontHud_, holdBuf, 960, barY + (isSplit ? 10 : 12), {18, 18, 18, 255}, true);
    } else {
        renderer_.drawText(isSplit ? fontSmall_ : fontHud_, "MANTÉN PRESIONADO [X] O [ENTER] PARA COMENZAR", 960, barY + (isSplit ? 10 : 12), {18, 18, 18, 255}, true);
    }
}

void UserInterface::renderDifficultySelect(Difficulty selected) {
    int modalW = 1260;
    int modalH = 680;
    int modalX = 960 - modalW / 2;
    int modalY = 540 - modalH / 2;

    renderer_.drawNeobrutalRect(modalX, modalY, modalW, modalH, {255, 255, 255, 255}, 6, 12);

    // Cabecera Neobrutalista
    int headerW = 1040;
    renderer_.drawNeobrutalRect(960 - headerW / 2, modalY - 30, headerW, 75, {56, 189, 248, 255}, 5, 8);
    renderer_.drawText(fontTitle_, "SELECCIONA EL NIVEL DE DIFICULTAD", 960, modalY - 22, {18, 18, 18, 255}, true);

    renderer_.drawText(fontHud_, "Usa el Volante o Flechas [Izq / Der] y presiona [X] o [ENTER]", 960, modalY + 68, {55, 65, 81, 255}, true);

    // 3 Tarjetas de dificultad
    const char* titles[3] = {"FÁCIL", "MEDIA", "DIFÍCIL"};
    const char* sub1[3] = {
        "• Shifter secuencial (Palanca)",
        "• Shifter secuencial (Palanca)",
        "• Todo lo de Media incluido"
    };
    const char* sub2[3] = {
        "• Motor indestructible",
        "• Daño por mal cambio",
        "• Botellas de Nitro / Turbo"
    };
    const char* sub3[3] = {
        "• Sin botones del Fight Stick",
        "• Enfriamiento de motor",
        "• Charcos y limpiaparabrisas"
    };
    SDL_Color cardTheme[3] = {
        {34, 197, 94, 255},   // Verde Esmeralda (Fácil)
        {250, 204, 21, 255},  // Amarillo Competición (Media)
        {239, 68, 68, 255}    // Rojo Fuego (Difícil)
    };

    int cardW = 380;
    int cardH = 360;
    int cardGap = 24;
    int totalCardsW = 3 * cardW + 2 * cardGap;
    int startX = 960 - totalCardsW / 2;
    int cardY = modalY + 120;

    for (int i = 0; i < 3; ++i) {
        bool isSel = (static_cast<int>(selected) == i);
        int cx = startX + i * (cardW + cardGap);
        int cy = isSel ? (cardY - 14) : cardY;

        // Fondo y borde
        SDL_Color bgCol = isSel ? SDL_Color{255, 255, 255, 255} : SDL_Color{243, 244, 246, 255};
        renderer_.drawNeobrutalRect(cx, cy, cardW, cardH, bgCol, isSel ? 5 : 3, isSel ? 10 : 5);

        // Cabecera de la tarjeta
        renderer_.drawNeobrutalRect(cx + 12, cy + 14, cardW - 24, 60, cardTheme[i], 3, 0);
        renderer_.drawText(fontTitle_, titles[i], cx + cardW / 2, cy + 22, {18, 18, 18, 255}, true);

        // Viñetas descriptivas
        renderer_.drawText(fontSmall_, sub1[i], cx + 18, cy + 110, {31, 41, 55, 255}, false);
        renderer_.drawText(fontSmall_, sub2[i], cx + 18, cy + 175, {31, 41, 55, 255}, false);
        renderer_.drawText(fontSmall_, sub3[i], cx + 18, cy + 240, {31, 41, 55, 255}, false);

        // Insignia inferior
        if (isSel) {
            renderer_.drawNeobrutalRect(cx + 30, cy + cardH - 52, cardW - 60, 38, {18, 18, 18, 255}, 2, 0);
            renderer_.drawText(fontSmall_, ">>> SELECCIONADO <<<", cx + cardW / 2, cy + cardH - 44, {254, 240, 138, 255}, true);
        } else {
            renderer_.drawText(fontSmall_, "Presiona para elegir", cx + cardW / 2, cy + cardH - 42, {156, 163, 175, 255}, true);
        }
    }

    // Botón Confirmar
    int btnW = 760;
    int btnH = 55;
    int btnY = modalY + modalH - 72;
    renderer_.drawNeobrutalRect(960 - btnW / 2, btnY, btnW, btnH, {34, 197, 94, 255}, 4, 6);
    renderer_.drawText(fontHud_, "CONFIRMAR DIFICULTAD Y COMENZAR [X]", 960, btnY + 12, {18, 18, 18, 255}, true);

    // Marca de agua arriba a la derecha en la selección de dificultad
    if (watermarkTex_) {
        SDL_Rect markRect = {1920 - 220, 28, 180, 139};
        SDL_RenderCopy(renderer_.getRenderer(), watermarkTex_, nullptr, &markRect);
    }
}

void UserInterface::renderCountdown(float countdownTimer) {
    // Semáforo Neobrutalista estilo Fórmula 1 en el centro de la pantalla
    // Secuencia de 4 segundos:
    // 3.0s a 4.0s: Semáforo 1 luz roja encendida
    // 2.0s a 3.0s: Semáforo 2 luces rojas encendidas
    // 1.0s a 2.0s: Semáforo 3 luces rojas encendidas
    // 0.0s a 1.0s: Semáforo en VERDE completo ("¡YA! / ¡GO!")
    if (countdownTimer < 0.0f) return;

    int boxW = 520;
    int boxH = 140;
    int boxX = 960 - boxW / 2;
    int boxY = 180;

    // Caja negra del semáforo con borde blanco / neobrutalista
    renderer_.drawNeobrutalRect(boxX, boxY, boxW, boxH, {18, 20, 26, 250}, 4, 8);

    // 5 Luces estilo F1
    int numLights = 5;
    int lightR = 28;
    int lightSpacing = 95;
    int startLightX = 960 - ((numLights - 1) * lightSpacing) / 2;
    int lightY = boxY + 50;

    bool isGo = (countdownTimer <= 1.0f);
    int activeReds = 0;
    if (!isGo) {
        if (countdownTimer > 3.0f) activeReds = 2;
        else if (countdownTimer > 2.0f) activeReds = 4;
        else activeReds = 5;
    }

    for (int i = 0; i < numLights; ++i) {
        int lx = startLightX + i * lightSpacing;
        // Borde exterior metálico
        renderer_.drawNeobrutalCircle(lx, lightY, lightR, {15, 23, 42, 255}, 3, 0);

        if (isGo) {
            // Todas las luces verdes brillantes al apagarse el rojo
            renderer_.drawNeobrutalCircle(lx, lightY, lightR - 4, {34, 197, 94, 255}, 0, 0);
            renderer_.drawNeobrutalCircle(lx, lightY, lightR / 2, {187, 247, 208, 255}, 0, 0); // Brillo
        } else {
            bool lit = (i < activeReds);
            if (lit) {
                // Luz roja encendida
                renderer_.drawNeobrutalCircle(lx, lightY, lightR - 4, {239, 68, 68, 255}, 0, 0);
                renderer_.drawNeobrutalCircle(lx, lightY, lightR / 2, {254, 202, 202, 255}, 0, 0);
            } else {
                // Luz apagada
                renderer_.drawNeobrutalCircle(lx, lightY, lightR - 4, {45, 20, 24, 255}, 0, 0);
            }
        }
    }

    // Texto dinámico inferior
    if (isGo) {
        renderer_.drawText(fontTitle_, "¡ ¡  V A M O S  ! !", 960, boxY + 95, {34, 197, 94, 255}, true);
    } else {
        int secDisplay = static_cast<int>(std::ceil(countdownTimer - 1.0f));
        char cBuf[32];
        std::snprintf(cBuf, sizeof(cBuf), "PREPARADOS... %d", std::max(1, secDisplay));
        renderer_.drawText(fontHud_, cBuf, 960, boxY + 95, {250, 204, 21, 255}, true);
    }
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

void UserInterface::renderTimeAttackHud(const Car& car, const Road& road, int currentLap, int totalLaps, float currentLapTime, float bestLapTime, float lastLapTime, int currentSector, float sectorDelta, float sectorBannerTimer, int completedSectorNotice, Difficulty diff) {
    // =========================================================================
    // HUD NEOBRUTALISTA DE VUELTA RÁPIDA / TIME ATTACK (F1 / Gran Turismo Style)
    // =========================================================================

    // 1. Contador de Vuelta - Esquina Superior Izquierda
    // Vuelta 1: Reconocimiento (Warmup) | Vuelta 2: Vuelta Rápida Oficial Cronometrada
    bool isRecon = (currentLap == 1);
    int lapBoxW = 400;
    int lapBoxH = 70;
    int lapBoxX = 50;
    int lapBoxY = 35;
    SDL_Color lapBadgeCol = isRecon ? SDL_Color{56, 189, 248, 255} : SDL_Color{250, 204, 21, 255};
    renderer_.drawNeobrutalRect(lapBoxX, lapBoxY, lapBoxW, lapBoxH, lapBadgeCol, 4, 6);
    
    if (isRecon) {
        renderer_.drawText(fontHud_, "V.1: RECONOCIMIENTO", lapBoxX + lapBoxW / 2, lapBoxY + 18, {18, 18, 18, 255}, true);
    } else {
        renderer_.drawText(fontHud_, "V.2: CRONOMETRADA", lapBoxX + lapBoxW / 2, lapBoxY + 18, {18, 18, 18, 255}, true);
    }

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
    SDL_Color tagCol = isRecon ? SDL_Color{56, 189, 248, 255} : SDL_Color{34, 197, 94, 255};
    const char* tagText = isRecon ? "CALENTAMIENTO" : "TIEMPO OFICIAL";
    int tagW = 210;
    renderer_.drawNeobrutalRect(960 - tagW / 2, timeBoxY - 14, tagW, 26, tagCol, 2, 0);
    renderer_.drawText(fontSmall_, tagText, 960, timeBoxY - 10, {18, 18, 18, 255}, true);

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

    // 4. Banner animado al cruzar Sector o Meta con Delta (+/- en verde o rojo)
    if (sectorBannerTimer > 0.0f) {
        bool isLap2Start = (currentLap == 2 && currentSector == 1 && completedSectorNotice == 0 && lastLapTime > 0.0f);
        int bW = isLap2Start ? 960 : 660;
        int bH = isLap2Start ? 82 : 75;
        int bannerX = 960 - bW / 2;
        int bannerY = 125;

        SDL_Color bannerCol = (sectorDelta <= 0.0f) ? SDL_Color{34, 197, 94, 255} : SDL_Color{239, 68, 68, 255};
        if (isLap2Start) {
            bannerCol = SDL_Color{250, 204, 21, 255}; // Amarillo para aviso de inicio de vuelta cronometrada
        }
        renderer_.drawNeobrutalRect(bannerX, bannerY, bW, bH, bannerCol, 4, 6);

        char deltaBuf[64];
        int secNum = (completedSectorNotice > 0) ? completedSectorNotice : currentSector;
        if (isLap2Start) {
            std::snprintf(deltaBuf, sizeof(deltaBuf), "¡VUELTA CRONOMETRADA INICIADA!");
        } else if (bestLapTime >= 9990.0f) {
            std::snprintf(deltaBuf, sizeof(deltaBuf), "SECTOR %d COMPLETADO", secNum);
        } else if (sectorDelta <= 0.0f) {
            std::snprintf(deltaBuf, sizeof(deltaBuf), "SECTOR %d  •  %.2fs", secNum, sectorDelta);
        } else {
            std::snprintf(deltaBuf, sizeof(deltaBuf), "SECTOR %d  •  +%.2fs", secNum, sectorDelta);
        }
        renderer_.drawText(fontTitle_, deltaBuf, 960, bannerY + (isLap2Start ? 17 : 14), {18, 18, 18, 255}, true);
    }

    // 4.1 Cartel Modernizado de Reducción de Velocidad / Curva Peligrosa (Apex Braking Indicator)
    // Analiza la curvatura real que se avecina en los próximos 10 a 28 segmentos (~120m a 340m adelante)
    int currentSegIdx = static_cast<int>(std::floor(car.getTrackZ() / Road::kSegmentLength));
    float maxAheadCurve = 0.0f;
    float curveDirection = 0.0f;
    for (int k = 8; k < 28; ++k) {
        float c = road.getSegment(currentSegIdx + k).curve;
        if (std::abs(c) > maxAheadCurve) {
            maxAheadCurve = std::abs(c);
            curveDirection = c;
        }
    }

    // Velocidad límite segura calculada según la física real del agarre de competición
    float safeSpeed = 260.0f;
    int suggestedGear = 5;
    if (maxAheadCurve > 5.8f) {
        safeSpeed = 135.0f; // Horquillas muy lentas
        suggestedGear = 3;
    } else if (maxAheadCurve > 4.5f) {
        safeSpeed = 165.0f; // Curvas medias y chicanes técnicas
        suggestedGear = 3;
    } else if (maxAheadCurve > 3.2f) {
        safeSpeed = 195.0f; // Curvones veloces
        suggestedGear = 4;
    }

    // Si el auto va más rápido de lo seguro para la curva que se avecina, activar cartel HUD deportivo
    if (car.getSpeed() > safeSpeed + 12.0f && maxAheadCurve >= 4.0f) {
        int warnW = 460;
        int warnH = 68;
        int warnX = 960 - warnW / 2;
        // Ubicación estratégica debajo del retrovisor y banner de sector, 100% visible sin tapar la pista
        int warnY = (sectorBannerTimer > 0.0f) ? 210 : 130;

        // Pulso animado de advertencia neobrutalista
        renderer_.drawNeobrutalRect(warnX, warnY, warnW, warnH, {239, 68, 68, 255}, 4, 6);
        renderer_.drawText(fontTitle_, "BAJA LA VELOCIDAD", 960, warnY + 11, {255, 255, 255, 255}, true);
    }

    // 5. Mini Mapa del Circuito Técnico (Esquina Derecha de la Pantalla)
    int mapW = 270;
    int mapH = 290;
    int mapX = 1920 - mapW - 40;
    int mapY = 140;
    renderMinimap(road, car.getTrackZ(), currentSector, mapX, mapY, mapW, mapH);

    // 6. Indicadores de Estado de Mecánicas de Cabina Arcade (Solo en Difícil para Nitro y Limpiaparabrisas)
    if (diff == Difficulty::HARD) {
        int badgeX = 50;
        int badgeY = 960;
        int badgeW = 400;
        int badgeH = 80;

        // Indicador de Botellas de Nitro
        SDL_Color nitroCol = car.isNitroActive() ? SDL_Color{250, 204, 21, 255} : SDL_Color{14, 165, 233, 255};
        renderer_.drawNeobrutalRect(badgeX, badgeY, badgeW, badgeH, {18, 20, 26, 255}, 4, 6);
        renderer_.drawNeobrutalRect(badgeX + 12, badgeY + 14, 52, 52, nitroCol, 2, 0);
        renderer_.drawText(fontSmall_, "N2O", badgeX + 38, badgeY + 28, {18, 18, 18, 255}, true);

        char nitroBuf[64];
        if (car.isNitroActive()) {
            std::snprintf(nitroBuf, sizeof(nitroBuf), "NITRO: ACTIVO (%.1fs)", car.getNitroTimer());
        } else {
            std::snprintf(nitroBuf, sizeof(nitroBuf), "TURBO / NITRO: x%d [X0]", car.getNitroInventory());
        }
        renderer_.drawText(fontSmall_, nitroBuf, badgeX + 78, badgeY + 16, nitroCol, false);
        renderer_.drawText(fontSmall_, "Limpia: [X3]  |  Enfría: [X8]", badgeX + 78, badgeY + 46, {148, 163, 184, 255}, false);
    }
}

void UserInterface::renderMinimap(const Road& road, float playerZ, int currentSector, int mapX, int mapY, int mapW, int mapH) {
    const auto& segments = road.getSegments();
    if (segments.empty()) return;

    // Caja Neobrutalista del Mini Mapa
    renderer_.drawNeobrutalRect(mapX, mapY, mapW, mapH, {20, 22, 28, 245}, 4, 6);

    // Encabezado de Sectores arriba del mapa
    renderer_.drawNeobrutalRect(mapX + 12, mapY + 10, mapW - 24, 32, {15, 23, 42, 255}, 2, 0);

    // Indicadores de los 3 sectores (S1, S2, S3)
    int secBtnW = (mapW - 32) / 3;
    const SDL_Color sectorColors[3] = {
        {37, 99, 235, 255},   // S1: Azul Real Intenso (Blue)
        {250, 204, 21, 255},  // S2: Amarillo Competición (Yellow)
        {239, 68, 68, 255}    // S3: Rojo F1 (Red)
    };

    for (int s = 1; s <= 3; ++s) {
        int sx = mapX + 16 + (s - 1) * secBtnW;
        bool isActive = (s == currentSector);
        SDL_Color bgCol = isActive ? sectorColors[s - 1] : SDL_Color{39, 42, 52, 255};
        SDL_Color textCol = isActive ? SDL_Color{255, 255, 255, 255} : SDL_Color{148, 163, 184, 255};
        renderer_.drawNeobrutalRect(sx, mapY + 13, secBtnW - 4, 26, bgCol, 1, 0);
        char sName[16];
        std::snprintf(sName, sizeof(sName), "S%d", s);
        renderer_.drawText(fontSmall_, sName, sx + (secBtnW - 4) / 2, mapY + 16, textCol, true);
    }

    // Bounding box precalculado directamente de las coordenadas 2D exactas del circuito
    static float minX = 0, maxX = 0, minY = 0, maxY = 0;
    static size_t lastSegCount = 0;

    int totalSegs = static_cast<int>(segments.size());
    if (lastSegCount != segments.size()) {
        minX = maxX = segments[0].mapX;
        minY = maxY = segments[0].mapY;
        for (const auto& seg : segments) {
            if (seg.mapX < minX) minX = seg.mapX;
            if (seg.mapX > maxX) maxX = seg.mapX;
            if (seg.mapY < minY) minY = seg.mapY;
            if (seg.mapY > maxY) maxY = seg.mapY;
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

    int s1End = totalSegs / 3;
    int s2End = (totalSegs * 2) / 3;

    // Dibujar el trazado real 1:1 de la pista con sus 3 sectores claramente diferenciados
    const int step = 2; // Trazar cada 2 segmentos para máxima fidelidad y suavidad
    for (int i = 0; i < totalSegs; i += step) {
        int nextI = (i + step) % totalSegs;
        float x1 = centerOffsetX + segments[i].mapX * scale;
        float y1 = centerOffsetY + segments[i].mapY * scale;
        float x2 = centerOffsetX + segments[nextI].mapX * scale;
        float y2 = centerOffsetY + segments[nextI].mapY * scale;

        int sec = (i < s1End) ? 0 : ((i < s2End) ? 1 : 2);
        SDL_Color lineColor = sectorColors[sec];
        // Resaltar con mayor grosor el sector por donde va el auto actualmente
        if (sec + 1 == currentSector) {
            renderer_.renderThickLine(x1, y1, x2, y2, 5.0f, lineColor);
        } else {
            renderer_.renderThickLine(x1, y1, x2, y2, 3.0f, lineColor);
        }
    }

    // Línea de meta sutil representada como pequeña barra perpendicular blanca
    if (totalSegs > 4) {
        float x0 = centerOffsetX + segments[0].mapX * scale;
        float y0 = centerOffsetY + segments[0].mapY * scale;
        float x1 = centerOffsetX + segments[4].mapX * scale;
        float y1 = centerOffsetY + segments[4].mapY * scale;
        float dx = x1 - x0;
        float dy = y1 - y0;
        float len = std::hypot(dx, dy);
        if (len > 0.001f) {
            float nx = -dy / len;
            float ny = dx / len;
            renderer_.renderThickLine(x0 - nx * 7.0f, y0 - ny * 7.0f, x0 + nx * 7.0f, y0 + ny * 7.0f, 3.5f, {255, 255, 255, 255});
        }
    }

    // Indicador dinámico de posición del coche: Triángulo chevron de competición apuntando en la dirección del viaje
    float exactSeg = playerZ / Road::kSegmentLength;
    int pIdx = static_cast<int>(std::floor(exactSeg)) % totalSegs;
    if (pIdx < 0) pIdx += totalSegs;
    float frac = exactSeg - std::floor(exactSeg);
    int nextPIdx = (pIdx + 1) % totalSegs;

    // Interpolación continua para que el auto se mueva con total fluidez analógica en el mapa
    float curX = centerOffsetX + (segments[pIdx].mapX * (1.0f - frac) + segments[nextPIdx].mapX * frac) * scale;
    float curY = centerOffsetY + (segments[pIdx].mapY * (1.0f - frac) + segments[nextPIdx].mapY * frac) * scale;

    int lookAheadIdx = (pIdx + 4) % totalSegs;
    float dirX = (centerOffsetX + segments[lookAheadIdx].mapX * scale) - curX;
    float dirY = (centerOffsetY + segments[lookAheadIdx].mapY * scale) - curY;
    float dirLen = std::hypot(dirX, dirY);

    if (dirLen > 0.001f) {
        float fwdX = dirX / dirLen;
        float fwdY = dirY / dirLen;
        float rightX = -fwdY;
        float rightY = fwdX;

        float arrowLen = 10.0f;
        float arrowHalfW = 6.5f;

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

void UserInterface::renderTimeAttackFinish(float bestLapTime, float totalSessionTime, float topSpeed, const std::vector<float>& lapTimes) {
    int modalW = 1200;
    int modalH = 680;
    int modalX = 960 - modalW / 2;
    int modalY = 540 - modalH / 2;

    renderer_.drawNeobrutalRect(modalX, modalY, modalW, modalH, {255, 255, 255, 255}, 6, 12);

    // Cabecera Neobrutalista
    int headerW = 1060;
    renderer_.drawNeobrutalRect(960 - headerW / 2, modalY - 30, headerW, 75, {250, 204, 21, 255}, 5, 8);
    renderer_.drawText(fontTitle_, "¡SESIÓN DE VUELTA RÁPIDA FINALIZADA!", 960, modalY - 22, {18, 18, 18, 255}, true);

    // Mejor Vuelta destacada en púrpura
    int bestBoxW = 740;
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
        char rowBuf[80];
        bool isBest = (std::abs(lt - bestLapTime) < 0.001f);
        if (i == 0) {
            std::snprintf(rowBuf, sizeof(rowBuf), "V.1 (RECONOCIMIENTO):  %02d:%05.2f", lm, ls);
        } else {
            std::snprintf(rowBuf, sizeof(rowBuf), "V.2 (CRONOMETRADA):   %02d:%05.2f %s", lm, ls, isBest ? " ★ (OFICIAL)" : "");
        }
        SDL_Color rowCol = (i > 0 && isBest) ? SDL_Color{34, 197, 94, 255} : ((i == 0) ? SDL_Color{56, 189, 248, 255} : SDL_Color{24, 24, 27, 255});
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
    int btnXW = 660;
    renderer_.drawNeobrutalRect(960 - btnXW / 2, modalY + 540, btnXW, 60, {34, 197, 94, 255}, 4, 6);
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
    int finishBtnW = 640;
    renderer_.drawNeobrutalRect(960 - finishBtnW / 2, modalY + 440, finishBtnW, 60, {250, 204, 21, 255}, 4, 6);
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
