#include "entity.h"
#include <cmath>
#include <cstdlib>

CyberEntity::CyberEntity(EntityType type, float x, float y, float vx, float vy, float idealAngle)
    : type_(type), x_(x), y_(y), vx_(vx), vy_(vy), idealAngle_(idealAngle) {
    
    switch (type_) {
        case EntityType::ENERGY_CORE:
            radius_ = 48.0f;
            rotSpeed_ = ((std::rand() % 200) - 100) / 50.0f;
            hasMandatoryAngle_ = false;
            break;
        case EntityType::CYBER_DRONE:
            radius_ = 54.0f;
            rotSpeed_ = 0.5f; // Rota suave para que el ángulo débil sea leíble
            hasMandatoryAngle_ = true;
            break;
        case EntityType::MECHA_SHURIKEN:
            radius_ = 42.0f;
            rotSpeed_ = 8.5f; // Rota a toda velocidad
            hasMandatoryAngle_ = false;
            break;
        case EntityType::EMP_MINE:
            radius_ = 50.0f;
            rotSpeed_ = 1.2f;
            hasMandatoryAngle_ = false;
            break;
    }
}

void CyberEntity::update(float dt) {
    if (!alive_) return;

    // Gravedad parabólica
    const float kGravity = 980.0f;
    vy_ += kGravity * dt;

    x_ += vx_ * dt;
    y_ += vy_ * dt;
    angle_ += rotSpeed_ * dt;

    // Si cae por debajo del borde de pantalla (con margen), muere
    if (y_ > 1250.0f) {
        alive_ = false;
    }
}

void CyberEntity::spawnHalves(float cutAngle, std::vector<CutHalf>& outHalves) const {
    // Vector perpendicular al corte para separar ambas mitades limpiamente
    float perpX = -std::sin(cutAngle);
    float perpY =  std::cos(cutAngle);
    float kick = 220.0f;

    CutHalf h1;
    h1.x = x_ + perpX * 12.0f;
    h1.y = y_ + perpY * 12.0f;
    h1.vx = vx_ * 0.4f + perpX * kick;
    h1.vy = vy_ * 0.4f + perpY * kick - 90.0f;
    h1.angle = angle_;
    h1.vRot = 4.0f;
    h1.radius = radius_;
    h1.life = 1.0f;
    h1.type = type_;
    h1.side = 1;
    h1.cutAngle = cutAngle;

    CutHalf h2;
    h2.x = x_ - perpX * 12.0f;
    h2.y = y_ - perpY * 12.0f;
    h2.vx = vx_ * 0.4f - perpX * kick;
    h2.vy = vy_ * 0.4f - perpY * kick - 90.0f;
    h2.angle = angle_;
    h2.vRot = -4.0f;
    h2.radius = radius_;
    h2.life = 1.0f;
    h2.type = type_;
    h2.side = -1;
    h2.cutAngle = cutAngle;

    outHalves.push_back(h1);
    outHalves.push_back(h2);
}
