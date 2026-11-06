#pragma once

#include <SDL.h>
#include <vector>

struct TouchPoint {
    float x = 0.0f;
    float y = 0.0f;
    float time = 0.0f;
};

struct SlashSegment {
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;
    float angleRad = 0.0f; // Dirección del corte en radianes
    float speed = 0.0f;    // Píxeles por segundo
    int playerIndex = 0;   // 0 = P1, 1 = P2
};

struct SlashTrailPoint {
    float x = 0.0f;
    float y = 0.0f;
    float life = 1.0f;     // 1.0 a 0.0
    float width = 14.0f;
};

class TouchInput {
public:
    TouchInput(int playerIndex, SDL_Color color);
    ~TouchInput();

    void setController(SDL_GameController* gc);
    SDL_GameController* getController() const { return controller_; }

    void update(float dt, int screenW, int screenH);
    void handleEvent(const SDL_Event& ev, int screenW, int screenH);

    const std::vector<SlashSegment>& getActiveSlashes() const { return activeSlashes_; }
    const std::vector<SlashTrailPoint>& getTrail() const { return trailPoints_; }
    SDL_Color getColor() const { return color_; }
    int getPlayerIndex() const { return playerIndex_; }

    bool isFingerDown() const { return fingerDown_; }
    float getFingerX() const { return currentX_; }
    float getFingerY() const { return currentY_; }

    void triggerHaptic(float low, float high, Uint32 ms);
    void setLED(Uint8 r, Uint8 g, Uint8 b);

private:
    int playerIndex_ = 0;
    SDL_Color color_;
    SDL_GameController* controller_ = nullptr;

    bool fingerDown_ = false;
    float currentX_ = 0.0f;
    float currentY_ = 0.0f;
    float prevX_ = 0.0f;
    float prevY_ = 0.0f;

    std::vector<TouchPoint> touchHistory_;
    std::vector<SlashSegment> activeSlashes_;
    std::vector<SlashTrailPoint> trailPoints_;
};
