#pragma once

#include "entity.h"
#include <vector>

class Spawner {
public:
    Spawner(int screenW, int screenH);

    void reset();
    void update(float dt, std::vector<CyberEntity>& outEntities);

    void setDifficultyMultiplier(float m) { difficulty_ = m; }

private:
    int screenW_ = 1920;
    int screenH_ = 1080;
    float timer_ = 0.0f;
    float spawnInterval_ = 1.6f;
    float gameTime_ = 0.0f;
    float difficulty_ = 1.0f;

    void spawnWave(std::vector<CyberEntity>& outEntities);
};

