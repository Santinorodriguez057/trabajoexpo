#pragma once

#include <SDL.h>
#include <string>
#include <vector>

enum class EntityType {
    ENERGY_CORE,    // Núcleo de energía esférico (normal, suma puntos)
    CYBER_DRONE,    // Robot volador / dron (con ángulo obligatorio de corte en su blindaje)
    MECHA_SHURIKEN, // Shuriken mecánico ultra-rápido (bonus dorado)
    EMP_MINE        // Mina PEM flotante con picos (¡NO CORTAR! Penaliza/quita vida)
};

struct CutHalf {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float angle = 0.0f;
    float vRot = 0.0f;
    float radius = 0.0f;
    float life = 1.0f;
    EntityType type;
    int side = 1; // +1 o -1 según la mitad
    float cutAngle = 0.0f;
};

class CyberEntity {
public:
    CyberEntity(EntityType type, float x, float y, float vx, float vy, float idealAngle = 0.0f);

    void update(float dt);
    bool isAlive() const { return alive_; }
    void kill() { alive_ = false; }

    EntityType getType() const { return type_; }
    float getX() const { return x_; }
    float getY() const { return y_; }
    float getRadius() const { return radius_; }
    float getAngle() const { return angle_; }
    float getIdealAngle() const { return idealAngle_; }
    bool hasMandatoryAngle() const { return hasMandatoryAngle_; }

    // Genera las dos mitades al ser cortado por un ángulo dado
    void spawnHalves(float cutAngle, std::vector<CutHalf>& outHalves) const;

private:
    EntityType type_;
    float x_ = 0.0f;
    float y_ = 0.0f;
    float vx_ = 0.0f;
    float vy_ = 0.0f;
    float radius_ = 48.0f;
    float angle_ = 0.0f;
    float rotSpeed_ = 0.0f;
    float idealAngle_ = 0.0f;
    bool hasMandatoryAngle_ = false;
    bool alive_ = true;
};
