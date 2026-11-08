#pragma once

#include "entity.h"
#include "input.h"
#include <cmath>
#include <algorithm>

enum class CutRating {
    NONE,
    GRAZE,   // Raspón / Ángulo pobre
    GOOD,    // Buen corte
    PERFECT  // Corte perfecto alineado
};

struct CollisionResult {
    bool hit = false;
    CutRating rating = CutRating::NONE;
    float cutAngle = 0.0f;
    int points = 0;
    bool isMine = false;
};

class CollisionSystem {
public:
    // Detección matemática precisa de colisión Segmento vs Círculo
    // P1: (x1, y1), P2: (x2, y2), Círculo: (cx, cy, radius)
    static bool checkSegmentCircle(float x1, float y1, float x2, float y2, float cx, float cy, float radius, float& outDist) {
        float dx = x2 - x1;
        float dy = y2 - y1;
        float lenSq = dx * dx + dy * dy;

        if (lenSq < 0.0001f) {
            float dist = std::hypot(cx - x1, cy - y1);
            outDist = dist;
            return dist <= radius;
        }

        // Proyección escalar 't' clampada a [0, 1]
        float t = ((cx - x1) * dx + (cy - y1) * dy) / lenSq;
        t = std::clamp(t, 0.0f, 1.0f);

        // Punto más cercano sobre el segmento
        float nearX = x1 + t * dx;
        float nearY = y1 + t * dy;

        float dist = std::hypot(cx - nearX, cy - nearY);
        outDist = dist;
        return dist <= radius;
    }

    // Evalúa si un corte impacta a la entidad y califica el ángulo
    static CollisionResult evaluateSlash(const SlashSegment& slash, const CyberEntity& entity) {
        CollisionResult res;
        float dist = 0.0f;
        float effectiveRadius = entity.getRadius() + (slash.thickness * 0.5f);

        if (!checkSegmentCircle(slash.x1, slash.y1, slash.x2, slash.y2,
                                entity.getX(), entity.getY(), effectiveRadius, dist)) {
            return res;
        }

        res.hit = true;
        res.cutAngle = slash.angleRad;

        // Si es una mina PEM, explota de inmediato
        if (entity.getType() == EntityType::EMP_MINE) {
            res.isMine = true;
            res.points = -150;
            res.rating = CutRating::GRAZE;
            return res;
        }

        // Si es una cápsula de power-up especial
        if (entity.getType() == EntityType::POWERUP_FREEZE ||
            entity.getType() == EntityType::POWERUP_DUAL ||
            entity.getType() == EntityType::POWERUP_FRENZY) {
            res.rating = CutRating::PERFECT;
            res.points = 150;
            return res;
        }

        // Si el objeto tiene un ángulo obligatorio (ej: Cyber Drone con blindaje direccional)
        if (entity.hasMandatoryAngle()) {
            // Se calcula la diferencia angular normalizada en [-PI, PI]
            // Dado que un corte en ángulo Theta o Theta + PI corta la misma línea, usamos simetría de línea
            float diff = std::abs(slash.angleRad - (entity.getIdealAngle() + entity.getAngle()));
            while (diff > 3.14159265f) diff -= 3.14159265f;
            float lineDiff = std::min(diff, std::abs(3.14159265f - diff)); // Distancia a la línea

            // Tolerancias angulares (en radianes):
            // Perfecto: < ~18 grados (~0.31 rad)
            // Bien:     < ~36 grados (~0.62 rad)
            // Fuera:    rebote o raspón
            if (lineDiff < 0.32f) {
                res.rating = CutRating::PERFECT;
                res.points = 250;
            } else if (lineDiff < 0.65f) {
                res.rating = CutRating::GOOD;
                res.points = 120;
            } else {
                res.rating = CutRating::GRAZE;
                res.points = 40;
            }
        } else {
            // Objeto normal (Núcleo) o Dorado (Shuriken)
            // Cuanto más cerca del centro pase el corte, mejor calificación
            float centerRatio = dist / entity.getRadius();
            if (centerRatio < 0.30f) {
                res.rating = CutRating::PERFECT;
                res.points = (entity.getType() == EntityType::MECHA_SHURIKEN) ? 350 : 180;
            } else if (centerRatio < 0.70f) {
                res.rating = CutRating::GOOD;
                res.points = (entity.getType() == EntityType::MECHA_SHURIKEN) ? 220 : 100;
            } else {
                res.rating = CutRating::GRAZE;
                res.points = 50;
            }
        }

        return res;
    }
};

