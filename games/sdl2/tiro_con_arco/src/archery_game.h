#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <vector>
#include <string>
#include "config.h"
#include "input.h"
#include "audio.h"
#include "theme.hpp"

struct TargetImpact {
    float localX; // Desplazamiento respecto al centro de la lata
    float localY;
    int targetIndex;
};

struct FlyingBullet {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float vz = 0.0f;
    float initialSpeed = 0.0f;
    bool active = false;
    std::vector<SDL_FPoint> trail;
    float impactFlash = 0.0f;
    SDL_FPoint impactPoint{0.0f, 0.0f};
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

struct MissMark {
    float x;
    float y;
    float life;
};

struct ConfettiPiece {
    float x;
    float y;
    float vx;
    float vy;
    float life;
    SDL_Color color;
    int size;
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
    void restartMatch();
    void shootGun();
    void evaluateImpact(float hitX, float hitY);

    // Helpers de renderizado
    void renderBackground();
    void renderFairTargets();
    void renderBullet();
    void renderGun();
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
    SDL_Point getMuzzleScreenPos() const;

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
    const int kShotsPerRound = 5;

    int currentRound_ = 1;
    int shotsRemaining_ = 5;
    int totalScore_ = 0;
    int roundScore_ = 0;
    int targetsDownThisRound_ = 0;
    int comboHits_ = 0;
    int totalHits_ = 0;
    int totalBullseyes_ = 0;
    int bestShotPoints_ = 0;
    bool isNewRecord_ = false;

    // Parámetros de la fila de latas en el puesto actual
    float targetDistance_ = 25.0f; // metros (Z)
    float targetX_ = 0.0f;         // metros (X)
    float targetY_ = 1.3f;         // metros (Y altura)
    float targetRadius_ = 0.52f;   // Radio de impacto, escalado para conservar tamaño aparente
    static constexpr int kTargetCount = 5;
    float targetScreenOffsetX_[kTargetCount] = {};
    float targetScreenOffsetY_[kTargetCount] = {};
    float targetBaseOffsetX_[kTargetCount] = {};
    float targetBaseOffsetY_[kTargetCount] = {};
    int targetKind_[kTargetCount] = {}; // 0 normal, 1 penalización, 2 dorado
    bool targetVisible_[kTargetCount] = {true, true, true, true, true};
    float targetFallAngle_[kTargetCount] = {};
    float targetFallVelocity_[kTargetCount] = {};
    bool targetFallen_[kTargetCount] = {};

    // Proyectil y marcas de impactos
    FlyingBullet activeBullet_;
    std::vector<TargetImpact> targetImpacts_;
    std::vector<FloatingText> floatingTexts_;
    std::vector<MissMark> missMarks_;
    std::vector<ConfettiPiece> confetti_;

    float muzzleFlashTimer_ = 0.0f;
    float shotRecoil_ = 0.0f;
    float reticlePulseTimer_ = 0.0f;
    float screenFlashTimer_ = 0.0f;
    int screenFlashAlpha_ = 0;
    int missFlashCount_ = 0;
    SDL_Color screenFlashColor_{255, 240, 210, 255};
    float weaponSwayX_ = 0.0f;

    // Menú de pausa
    int pauseMenuIndex_ = 0;
    int lastHudScore_ = -1;

    // Variables de tiempo y temblor
    float roundTime_ = 0.0f;
    float screenShakeTrauma_ = 0.0f;
    float fairCelebrationTimer_ = 0.0f;
    float hudEntryTimer_ = 0.0f;
    float scorePulseTimer_ = 0.0f;
    float roundBannerTimer_ = 0.0f;
    float roundEndTimer_ = 0.0f;
    bool roundEnding_ = false;
};
