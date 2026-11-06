#include "score.h"
#include <algorithm>

ScoreSystem::ScoreSystem() {
    reset();
}

void ScoreSystem::reset() {
    p1Score_ = 0;
    p2Score_ = 0;
    p1Combo_ = 0;
    p2Combo_ = 0;
    p1ComboTimer_ = 0.0f;
    p2ComboTimer_ = 0.0f;
    lives_ = 3;
    floatingScores_.clear();
}

void ScoreSystem::addCut(int playerIndex, int basePoints, bool isPerfect, float x, float y) {
    int& score = (playerIndex == 0) ? p1Score_ : p2Score_;
    int& combo = (playerIndex == 0) ? p1Combo_ : p2Combo_;
    float& cTimer = (playerIndex == 0) ? p1ComboTimer_ : p2ComboTimer_;

    combo++;
    cTimer = 1.4f; // Ventana para encadenar combos

    int multiplier = std::min(6, 1 + (combo / 2));
    int finalPoints = basePoints * multiplier;
    score += finalPoints;

    FloatingScore fs;
    fs.x = x;
    fs.y = y - 10.0f;
    if (isPerfect) {
        fs.text = "¡PERFECTO! +" + std::to_string(finalPoints);
        fs.r = 34; fs.g = 197; fs.b = 94; // Verde esmeralda
    } else if (combo > 1) {
        fs.text = "x" + std::to_string(combo) + " COMBO +" + std::to_string(finalPoints);
        fs.r = 250; fs.g = 204; fs.b = 21; // Amarillo
    } else {
        fs.text = "+" + std::to_string(finalPoints);
        fs.r = 245; fs.g = 245; fs.b = 250; // Blanco
    }
    fs.life = 1.0f;
    floatingScores_.push_back(fs);
}

void ScoreSystem::registerMineHit(int playerIndex, float x, float y) {
    int& score = (playerIndex == 0) ? p1Score_ : p2Score_;
    int& combo = (playerIndex == 0) ? p1Combo_ : p2Combo_;

    combo = 0;
    score = std::max(0, score - 150);
    loseLife();

    FloatingScore fs;
    fs.x = x;
    fs.y = y;
    fs.text = "¡MINA PEM! -150";
    fs.r = 239; fs.g = 68; fs.b = 68; // Rojo
    fs.life = 1.2f;
    floatingScores_.push_back(fs);
}

void ScoreSystem::registerDropMiss(float x, float y) {
    p1Combo_ = 0;
    loseLife();

    FloatingScore fs;
    fs.x = x;
    fs.y = y;
    fs.text = "¡FALLO! -1 VIDA";
    fs.r = 239; fs.g = 68; fs.b = 68;
    fs.life = 1.2f;
    floatingScores_.push_back(fs);
}

void ScoreSystem::update(float dt) {
    if (p1ComboTimer_ > 0.0f) {
        p1ComboTimer_ -= dt;
        if (p1ComboTimer_ <= 0.0f) p1Combo_ = 0;
    }
    if (p2ComboTimer_ > 0.0f) {
        p2ComboTimer_ -= dt;
        if (p2ComboTimer_ <= 0.0f) p2Combo_ = 0;
    }

    // Actualizar textos flotantes
    for (auto& fs : floatingScores_) {
        fs.y -= 45.0f * dt;
        fs.life -= dt * 1.5f;
    }
    floatingScores_.erase(
        std::remove_if(floatingScores_.begin(), floatingScores_.end(),
                       [](const FloatingScore& fs) { return fs.life <= 0.0f; }),
        floatingScores_.end());
}

