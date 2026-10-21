#include "ui.h"
#include <cstdio>
#include <algorithm>
#include <vector>

UserInterface::UserInterface(Renderer& renderer, TTF_Font* fontTitle, TTF_Font* fontHud, TTF_Font* fontSmall, DrivingConfig& config)
    : renderer_(renderer), fontTitle_(fontTitle), fontHud_(fontHud), fontSmall_(fontSmall), config_(config) {
}

void UserInterface::renderCalibration(const InputState& input) {
    // Modal central Neobrutalista
    int modalW = 1200;
    int modalH = 740;
    int modalX = 960 - modalW / 2;
    int modalY = 540 - modalH / 2;

    renderer_.drawNeobrutalRect(modalX, modalY, modalW, modalH, {255, 255, 255, 255}, 6, 12);

    // Cabecera Neobrutalista Amarilla
    renderer_.drawNeobrutalRect(modalX + 40, modalY - 30, modalW - 80, 75, {250, 204, 21, 255}, 5, 8);
    renderer_.drawText(fontTitle_, "CALIBRACIÓN DEL VOLANTE DUALSHOCK", 960, modalY - 22, {18, 18, 18, 255}, true);

    // Instrucciones
    renderer_.drawText(fontHud_, "Sostén el mando frente a ti como un volante de carreras.", 960, modalY + 80, {24, 24, 27, 255}, true);
    renderer_.drawText(fontSmall_, "Inclina el mando a los lados para girar el volante estilo Gran Turismo.", 960, modalY + 130, {75, 85, 99, 255}, true);

    // Cuadro interactivo con volante girando en vivo
    int previewBoxW = 440;
    int previewBoxH = 260;
    int previewBoxX = 960 - previewBoxW / 2;
    int previewBoxY = modalY + 180;
    renderer_.drawNeobrutalRect(previewBoxX, previewBoxY, previewBoxW, previewBoxH, {30, 41, 59, 255}, 4, 6);

    // Volante miniatura interactivo
    int pWheelRadius = 75;
    int pWheelCenterY = previewBoxY + previewBoxH / 2 + 10;
    renderer_.drawNeobrutalCircle(960, pWheelCenterY, pWheelRadius, {18, 18, 18, 255}, 5, 4);
    renderer_.drawNeobrutalCircle(960, pWheelCenterY, pWheelRadius - 22, {30, 41, 59, 255}, 3, 0);
    renderer_.drawNeobrutalCircle(960, pWheelCenterY, 24, {244, 63, 94, 255}, 3, 0);

    // Franja indicadora superior rotada
    float rad = input.physicalAngleDeg * 0.0174532925f - 1.57079f;
    int tapeX = 960 + static_cast<int>(std::cos(rad) * (pWheelRadius - 11));
    int tapeY = pWheelCenterY + static_cast<int>(std::sin(rad) * (pWheelRadius - 11));
    renderer_.drawNeobrutalCircle(tapeX, tapeY, 10, {250, 204, 21, 255}, 2, 0);

    // Telemetría en vivo
    char angBuf[64];
    std::snprintf(angBuf, sizeof(angBuf), "ÁNGULO DEL VOLANTE: %+.1f°", input.physicalAngleDeg);
    renderer_.drawText(fontSmall_, angBuf, 960, previewBoxY + 15, {250, 204, 21, 255}, true);

    // Controles
    int cY = modalY + 475;
    renderer_.drawText(fontSmall_, "• Acelerar: Gatillo R2 (o Tecla W / Flecha Arriba)", 960, cY, {18, 18, 18, 255}, true);
    renderer_.drawText(fontSmall_, "• Frenar / Reversa: Gatillo L2 (o Tecla S / Flecha Abajo)", 960, cY + 35, {18, 18, 18, 255}, true);
    renderer_.drawText(fontSmall_, "• Recalibrar Centro: Botón Triángulo [△] o Tecla [C]", 960, cY + 70, {18, 18, 18, 255}, true);
    renderer_.drawText(fontSmall_, "• Salir al Menú: Botón Círculo [O] o Tecla [ESC]", 960, cY + 105, {239, 68, 68, 255}, true);

    // Botón de confirmación
    renderer_.drawNeobrutalRect(960 - 280, modalY + modalH - 55, 560, 65, {34, 197, 94, 255}, 5, 8);
    renderer_.drawText(fontHud_, "PRESIONA [X] O [ENTER] PARA COMENZAR", 960, modalY + modalH - 45, {18, 18, 18, 255}, true);
}

void UserInterface::renderHud(const Car& car, float timeRemaining, int checkpointNum, float bonusBannerTimer) {
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
}

void UserInterface::renderFinish(float totalTime, float topSpeed, int checkpointsPassed, bool victory) {
    int modalW = 1000;
    int modalH = 650;
    int modalX = 960 - modalW / 2;
    int modalY = 540 - modalH / 2;

    SDL_Color headerCol = victory ? SDL_Color{34, 197, 94, 255} : SDL_Color{239, 68, 68, 255};
    renderer_.drawNeobrutalRect(modalX, modalY, modalW, modalH, {255, 255, 255, 255}, 6, 12);

    renderer_.drawNeobrutalRect(modalX + 50, modalY - 30, modalW - 100, 75, headerCol, 5, 8);
    std::string title = victory ? "¡CARRERA COMPLETADA!" : "¡TIEMPO AGOTADO!";
    renderer_.drawText(fontTitle_, title, 960, modalY - 22, {18, 18, 18, 255}, true);

    // Estadísticas
    char timeBuf[64];
    std::snprintf(timeBuf, sizeof(timeBuf), "Tiempo Total: %.2f segundos", totalTime);
    renderer_.drawText(fontHud_, timeBuf, 960, modalY + 100, {24, 24, 27, 255}, true);

    char spdBuf[64];
    std::snprintf(spdBuf, sizeof(spdBuf), "Velocidad Máxima: %.1f km/h", topSpeed);
    renderer_.drawText(fontHud_, spdBuf, 960, modalY + 170, {24, 24, 27, 255}, true);

    std::string cpBuf = "Checkpoints Superados: " + std::to_string(checkpointsPassed);
    renderer_.drawText(fontHud_, cpBuf, 960, modalY + 240, {24, 24, 27, 255}, true);

    // Rango
    std::string rank = victory ? (totalTime < 95.0f ? "RANGO: S (LEGENDARIO)" : "RANGO: A (PILOTO PRO)") : "RANGO: B";
    renderer_.drawText(fontTitle_, rank, 960, modalY + 340, {244, 63, 94, 255}, true);

    // Botones
    renderer_.drawNeobrutalRect(960 - 260, modalY + 460, 520, 60, {250, 204, 21, 255}, 4, 6);
    renderer_.drawText(fontHud_, "Presiona [X] para Jugar de Nuevo", 960, modalY + 472, {18, 18, 18, 255}, true);

    renderer_.drawText(fontSmall_, "Presiona [O] o [ESC] para Salir al Menú", 960, modalY + 560, {107, 114, 128, 255}, true);
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
