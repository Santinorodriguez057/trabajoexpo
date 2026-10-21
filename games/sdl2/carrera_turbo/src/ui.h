#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <string>
#include "renderer.h"
#include "input.h"
#include "car.h"

enum class GameState {
    CALIBRATION,
    RACING,
    CHECKPOINT_BONUS,
    FINISH,
    GAME_OVER,
    PAUSE
};

class UserInterface {
public:
    UserInterface(Renderer& renderer, TTF_Font* fontTitle, TTF_Font* fontHud, TTF_Font* fontSmall, DrivingConfig& config);

    void renderCalibration(const InputState& input);
    void renderHud(const Car& car, float timeRemaining, int checkpointNum, float bonusBannerTimer);
    void renderFinish(float totalTime, float topSpeed, int checkpointsPassed, bool victory);
    void renderPause(int selectedIndex);

private:
    Renderer& renderer_;
    TTF_Font* fontTitle_;
    TTF_Font* fontHud_;
    TTF_Font* fontSmall_;
    DrivingConfig& config_;
};
