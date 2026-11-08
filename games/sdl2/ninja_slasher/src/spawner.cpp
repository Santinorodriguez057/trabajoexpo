#include "spawner.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>

Spawner::Spawner(int screenW, int screenH)
    : screenW_(screenW), screenH_(screenH) {
    reset();
}

void Spawner::reset() {
    timer_ = 0.5f;
    spawnInterval_ = 1.7f;
    gameTime_ = 0.0f;
    difficulty_ = 1.0f;
}

void Spawner::update(float dt, std::vector<CyberEntity>& outEntities) {
    gameTime_ += dt;
    timer_ -= dt;

    // Dificultad progresiva: con el tiempo se acelera el intervalo de aparición
    float targetInterval = std::max(0.75f, 1.8f - (gameTime_ * 0.015f));
    spawnInterval_ = targetInterval / difficulty_;

    if (timer_ <= 0.0f) {
        spawnWave(outEntities);
        timer_ = spawnInterval_;
    }
}

void Spawner::spawnWave(std::vector<CyberEntity>& outEntities) {
    // Generar entre 1 y 3 objetos por oleada según el progreso
    int count = 1;
    if (gameTime_ > 15.0f && (std::rand() % 100 < 60)) count = 2;
    if (gameTime_ > 35.0f && (std::rand() % 100 < 40)) count = 3;

    for (int i = 0; i < count; ++i) {
        // Lanzamiento desde la parte inferior con impulso parabólico hacia el centro
        float spawnX = 350.0f + (std::rand() % (screenW_ - 700));
        float spawnY = screenH_ + 70.0f + (i * 35.0f);

        // Velocidad vertical para que suba hasta un apex jugable (~200px a 400px desde arriba)
        float targetApexY = 180.0f + (std::rand() % 240);
        float deltaH = spawnY - targetApexY;
        float vy = -std::sqrt(2.0f * 980.0f * deltaH);

        // Velocidad horizontal hacia el tercio central
        float centerX = screenW_ * 0.5f + ((std::rand() % 400) - 200);
        float timeToApex = -vy / 980.0f;
        float vx = (centerX - spawnX) / std::max(0.5f, timeToApex * 1.6f);

        // Probabilidad de tipos de objetos
        int roll = std::rand() % 100;
        EntityType type = EntityType::ENERGY_CORE;
        float idealAngle = 0.0f;

        if (roll < 40) {
            type = EntityType::ENERGY_CORE;
        } else if (roll < 65) {
            type = EntityType::CYBER_DRONE;
            // Ángulo obligatorio aleatorio: 0°, 45°, 90°, 135°
            int angStep = std::rand() % 4;
            idealAngle = angStep * (3.14159265f / 4.0f);
        } else if (roll < 80) {
            type = EntityType::EMP_MINE; // Peligro / mina
        } else if (roll < 90) {
            type = EntityType::MECHA_SHURIKEN; // Bonus oro rápido
            vx *= 1.35f;
        } else {
            // Cápsula de Power-up (10% de probabilidad total)
            int pRoll = std::rand() % 3;
            if (pRoll == 0) type = EntityType::POWERUP_FREEZE;
            else if (pRoll == 1) type = EntityType::POWERUP_DUAL;
            else type = EntityType::POWERUP_FRENZY;
            vx *= 0.85f; // Flotan un poco más pausadas para favorecer el corte
        }

        outEntities.emplace_back(type, spawnX, spawnY, vx, vy, idealAngle);
    }
}

