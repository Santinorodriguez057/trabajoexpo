#pragma once

#include <string>
#include <vector>

struct FloatingScore {
    float x = 0.0f;
    float y = 0.0f;
    std::string text;
    float life = 1.0f;
    int r = 255, g = 255, b = 255;
};

class ScoreSystem {
public:
    ScoreSystem();

    void reset();
    void addCut(int playerIndex, int basePoints, bool isPerfect, float x, float y);
    void registerMineHit(int playerIndex, float x, float y);
    void registerDropMiss(float x, float y);
    void update(float dt);

    int getScore(int playerIndex) const { return (playerIndex == 0) ? p1Score_ : p2Score_; }
    int getCombo(int playerIndex) const { return (playerIndex == 0) ? p1Combo_ : p2Combo_; }
    int getLives() const { return lives_; }
    bool isGameOver() const { return lives_ <= 0; }
    void loseLife() { if (lives_ > 0) --lives_; }

    const std::vector<FloatingScore>& getFloatingScores() const { return floatingScores_; }

private:
    int p1Score_ = 0;
    int p2Score_ = 0;
    int p1Combo_ = 0;
    int p2Combo_ = 0;
    float p1ComboTimer_ = 0.0f;
    float p2ComboTimer_ = 0.0f;

    int lives_ = 3;

    std::vector<FloatingScore> floatingScores_;
};

