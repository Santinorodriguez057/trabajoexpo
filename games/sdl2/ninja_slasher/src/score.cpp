#include "score.h"
#include <algorithm>
#include <fstream>
#include <cstdio>

namespace {
    std::string resolveRecordFile(bool forWrite) {
        const char* candidates[] = {
            "ninja_slasher_record.dat",
            "../ninja_slasher_record.dat",
            "../../ninja_slasher_record.dat"
        };
        for (const char* path : candidates) {
            if (FILE* f = std::fopen(path, "rb")) {
                std::fclose(f);
                return path;
            }
        }
        return forWrite ? "ninja_slasher_record.dat" : "";
    }
}

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
    multiBanners_.clear();
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
        fs.scaleMul = 1.12f;
    } else if (combo > 1) {
        fs.text = "x" + std::to_string(combo) + " COMBO +" + std::to_string(finalPoints);
        fs.r = 250; fs.g = 204; fs.b = 21; // Amarillo
        fs.scaleMul = 1.0f;
    } else {
        fs.text = "+" + std::to_string(finalPoints);
        fs.r = 245; fs.g = 245; fs.b = 250; // Blanco
        fs.scaleMul = 0.95f;
    }
    fs.life = 1.0f;
    floatingScores_.push_back(fs);
}

int ScoreSystem::addMultiSliceBonus(int playerIndex, int sliceCount, float x, float y) {
    if (sliceCount < 2) return 0;
    int& score = (playerIndex == 0) ? p1Score_ : p2Score_;
    int bonus = sliceCount * 120 + (sliceCount >= 3 ? (sliceCount - 2) * 100 : 0);
    score += bonus;

    // Si ya hay un banner reciente del mismo jugador en este gesto, actualizarlo en vez de apilar
    for (auto& mb : multiBanners_) {
        if (mb.playerIndex == playerIndex && mb.life > 0.65f) {
            mb.count = std::max(mb.count, sliceCount);
            mb.bonus = bonus;
            mb.x = std::clamp(x, 260.0f, 1660.0f);
            mb.y = std::clamp(y - 55.0f, 190.0f, 860.0f);
            mb.life = 1.0f;
            return bonus;
        }
    }

    MultiSliceBanner mb;
    mb.x = std::clamp(x, 260.0f, 1660.0f);
    mb.y = std::clamp(y - 55.0f, 190.0f, 860.0f);
    mb.count = sliceCount;
    mb.bonus = bonus;
    mb.playerIndex = playerIndex;
    mb.life = 1.0f;
    multiBanners_.push_back(mb);
    return bonus;
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
    fs.scaleMul = 1.18f;
    floatingScores_.push_back(fs);
}

void ScoreSystem::registerDropMiss(float x, float y) {
    p1Combo_ = 0;
    loseLife();

    FloatingScore fs;
    fs.x = std::clamp(x, 180.0f, 1740.0f);
    fs.y = y;
    fs.text = "¡ESCAPÓ! -1 ESCUDO";
    fs.r = 239; fs.g = 68; fs.b = 68;
    fs.life = 1.2f;
    fs.scaleMul = 1.08f;
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

    for (auto& fs : floatingScores_) {
        fs.y -= 48.0f * dt;
        fs.life -= dt * 1.45f;
    }
    floatingScores_.erase(
        std::remove_if(floatingScores_.begin(), floatingScores_.end(),
                       [](const FloatingScore& fs) { return fs.life <= 0.0f; }),
        floatingScores_.end());

    for (auto& mb : multiBanners_) {
        mb.y -= 28.0f * dt;
        mb.life -= dt * 0.95f;
    }
    multiBanners_.erase(
        std::remove_if(multiBanners_.begin(), multiBanners_.end(),
                       [](const MultiSliceBanner& mb) { return mb.life <= 0.0f; }),
        multiBanners_.end());
}

int ScoreSystem::loadBestScore() {
    std::string path = resolveRecordFile(false);
    if (path.empty()) return 0;
    std::ifstream in(path);
    int best = 0;
    if (in >> best) {
        return std::max(0, best);
    }
    return 0;
}

void ScoreSystem::saveBestScore(int best) {
    if (best <= 0) return;
    std::string path = resolveRecordFile(true);
    std::ofstream out(path, std::ios::trunc);
    if (out.is_open()) {
        out << best << "\n";
    }
}
