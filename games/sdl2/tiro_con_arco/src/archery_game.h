#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <vector>
#include <string>
#include "config.h"
#include "input.h"
#include "audio.h"
#include "theme.hpp"

struct StuckArrow {
    float localX; // Desplazamiento respecto al centro de la diana
    float localY;
    float angle;  // Ángulo de inclinación de la flecha clavada
};

struct FlyingArrow {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float vz = 0.0f;
    float initialSpeed = 0.0f;
    bool active = false;
    std::vector<SDL_FPoint> trail;
};

struct FloatingText {
    std::string text;
    float x;
    float y;
    float life; // segundos restantes
    float maxLife;
    SDL_Color color;
    float scale;
};

enum class GameState {
    CALIBRATION,
    PLAYING,
    ROUND_SUMMARY,
    MATCH_OVER,
    PAUSE_MENU
};

class ArcheryGame {
public:
    ArcheryGame(SDL_Renderer* ren, TTF_Font* fontLarge, TTF_Font* fontMed, TTF_Font* fontSmall, GameConfig& config);
    ~ArcheryGame();

    int run();

private:
    void handleEvents();
    void update(float dt);
    void render();

    void startRound(int roundNumber);
    void shootArrow(float tension);
    void evaluateImpact(float hitX, float hitY);

    // Helpers de renderizado
    void renderBackground();
    void renderTarget();
    void renderFlyingArrow();
    void renderFirstPersonBow();
    void renderHUD();
    void renderCalibrationScreen();
    void renderRoundSummary();
    void renderMatchOver();
    void renderPauseMenu();

    void drawText(TTF_Font* font, const std::string& text, int x, int y, SDL_Color color, bool center = false);
    void drawCircleFilled(int cx, int cy, int radius, SDL_Color color);
    void drawCircleOutline(int cx, int cy, int radius, SDL_Color color);
    void drawFlat(float cx, float cy, float w, float h, float angle, SDL_Color color);

    // Proyección 3D a pantalla 1920x1080
    SDL_Point project3D(float x, float y, float z);

    SDL_Renderer* ren_;
    TTF_Font* fontLarge_;
    TTF_Font* fontMed_;
    TTF_Font* fontSmall_;
    GameConfig& config_;

    InputManager input_;
    SoundManager audio_;

    GameState state_ = GameState::CALIBRATION;
    bool running_ = true;

    // Configuración de partida
    const int kTotalRounds = 10;
    const int kArrowsPerRound = 5;

    int currentRound_ = 1;
    int arrowsRemaining_ = 5;
    int totalScore_ = 0;
    int roundScore_ = 0;
    int totalHits_ = 0;
    int totalBullseyes_ = 0;
    int bestShotPoints_ = 0;
    bool isNewRecord_ = false;

    // Parámetros de la diana en la ronda actual
    float targetDistance_ = 25.0f; // metros (Z)
    float targetX_ = 0.0f;         // metros (X)
    float targetY_ = 1.3f;         // metros (Y altura)
    float targetRadius_ = 0.61f;   // 122cm de diámetro (oficial FITA)
    float targetWobble_ = 0.0f;    // oscilación por impacto

    // Flechas
    FlyingArrow activeArrow_;
    std::vector<StuckArrow> stuckArrows_;
    std::vector<FloatingText> floatingTexts_;

    // Animación de cuerda del arco
    float stringVibration_ = 0.0f;
    float stringVibeTimer_ = 0.0f;

    // Menú de pausa
    int pauseMenuIndex_ = 0;

    // Variables de tiempo, viento y temblor
    float roundTime_ = 0.0f;
    float screenShakeTrauma_ = 0.0f;
    float windSpeed_ = 0.0f;
    float windX_ = 0.0f;
    float windY_ = 0.0f;
    std::string windLabel_ = "0.0 M/S";
};
