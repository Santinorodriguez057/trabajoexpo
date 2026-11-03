#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <string>
#include "renderer.h"
#include "input.h"
#include "car.h"

enum class Difficulty {
    EASY,     // Solo movimiento, marchas automáticas, sin mecánicas fight stick
    MEDIUM,   // Agrega marchas manuales secuenciales y mecánica de mal cambio/motor roto
    HARD      // Agrega además Turbo/Nitro y Charcos con Limpiaparabrisas
};

enum class GameState {
    CALIBRATION,
    DIFFICULTY_SELECT,
    COUNTDOWN,
    RACING,
    CHECKPOINT_BONUS,
    FINISH,
    GAME_OVER,
    PAUSE
};

class UserInterface {
public:
    UserInterface(Renderer& renderer, TTF_Font* fontTitle, TTF_Font* fontHud, TTF_Font* fontSmall, DrivingConfig& config);
    ~UserInterface();

    void renderCalibration(const InputState& input, int playerNum = 1, bool isSplit = false);
    void renderDifficultySelect(Difficulty selected);
    void renderCountdown(float countdownTimer);
    void renderHud(const Car& car, float timeRemaining, int checkpointNum, float bonusBannerTimer, bool isSplit = false, int playerNum = 1, float rivalGap = 0.0f);
    void renderTimeAttackHud(const Car& car, const Road& road, int currentLap, int totalLaps, float currentLapTime, float bestLapTime, float lastLapTime, int currentSector, float sectorDelta, float sectorBannerTimer, Difficulty diff = Difficulty::HARD);
    void renderMinimap(const Road& road, float playerZ, int currentSector, int mapX, int mapY, int mapW, int mapH);
    void renderFinish(float totalTime, float topSpeed, int checkpointsPassed, bool victory, bool isSplit = false, int winnerPlayer = 0);
    void renderTimeAttackFinish(float bestLapTime, float totalSessionTime, float topSpeed, const std::vector<float>& lapTimes);
    void renderPause(int selectedIndex);

private:
    void renderDualShockDiagram(int cx, int cy, float tiltAngleDeg, bool isSplit);

    Renderer& renderer_;
    TTF_Font* fontTitle_;
    TTF_Font* fontHud_;
    TTF_Font* fontSmall_;
    DrivingConfig& config_;

    SDL_Texture* controllerTex_ = nullptr;
};
