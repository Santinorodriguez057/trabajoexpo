#include "road.h"
#include <cmath>
#include <cstdlib>

Road::Road() {
    generateTrack();
}

void Road::addSegment(float curve, float y) {
    int n = static_cast<int>(segments_.size());
    RoadSegment seg;
    seg.index = n;
    seg.worldZ = n * kSegmentLength;
    seg.worldY = y;
    seg.curve = curve;

    // Paleta Neobrutalista saturada con bandas alternadas
    bool alt = (n / 3) % 2 == 0;
    if (alt) {
        seg.grassColor = {34, 197, 94, 255};   // Verde esmeralda vivo
        seg.rumbleColor = {239, 68, 68, 255};  // Rojo neón
        seg.roadColor = {24, 24, 27, 255};     // Asfalto negro profundo
        seg.laneColor = {255, 255, 255, 255};  // Línea blanca
    } else {
        seg.grassColor = {74, 222, 128, 255};  // Verde lima brillante
        seg.rumbleColor = {255, 255, 255, 255};// Blanco puro
        seg.roadColor = {39, 39, 42, 255};     // Asfalto gris oscuro
        seg.laneColor = {39, 39, 42, 255};     // Sin línea en alternancia
    }

    segments_.push_back(seg);
}

void Road::addStraight(int num) {
    for (int i = 0; i < num; ++i) {
        float lastY = segments_.empty() ? 0.0f : segments_.back().worldY;
        addSegment(0.0f, lastY);
    }
}

void Road::addCurve(int num, float curve, float endY) {
    float startY = segments_.empty() ? 0.0f : segments_.back().worldY;
    for (int i = 0; i < num; ++i) {
        float t = static_cast<float>(i) / num;
        // Curva suave senoidal para entrada y salida natural
        float c = std::sin(t * 3.14159f) * curve;
        float y = startY + (endY - startY) * (0.5f - 0.5f * std::cos(t * 3.14159f));
        addSegment(c, y);
    }
}

void Road::addHill(int num, float height) {
    float startY = segments_.empty() ? 0.0f : segments_.back().worldY;
    for (int i = 0; i < num; ++i) {
        float t = static_cast<float>(i) / num;
        float y = startY + std::sin(t * 3.14159f) * height;
        addSegment(0.0f, y);
    }
}

void Road::generateTrack() {
    segments_.clear();
    traffic_.clear();

    // 1. Recta de Salida
    addStraight(60);

    // 2. Curva Amplia a la Derecha con palmeras
    addCurve(120, 2.5f, 0.0f);

    // 3. Colina suave y bajada
    addHill(100, 450.0f);

    // 4. Curva en S (Izquierda luego Derecha)
    addCurve(90, -3.2f, 0.0f);
    addStraight(30);
    addCurve(90, 3.0f, 0.0f);

    // 5. Recta veloz con colina escarpada
    addHill(80, -350.0f);
    addStraight(70);

    // 6. Curva cerrada a la izquierda
    addCurve(110, -4.0f, 0.0f);

    // 7. Chicane técnica
    addCurve(50, 3.8f, 0.0f);
    addCurve(50, -3.8f, 0.0f);

    // 8. Recta final hacia la meta
    addStraight(100);

    trackLength_ = segments_.size() * kSegmentLength;

    // Decoración de atrezzo neobrutalista a los lados de la pista
    for (size_t i = 10; i < segments_.size(); i += 8) {
        if (i % 60 == 0) {
            // Arcos de Checkpoint cada ~700m
            segments_[i].props.push_back({PropType::CHECKPOINT_ARCH, 0.0f});
            segments_[i].isCheckpoint = true;
        } else if (i % 24 == 0) {
            // Carteles Neobrutalistas
            float side = ((i / 24) % 2 == 0) ? -1.65f : 1.65f;
            segments_[i].props.push_back({PropType::BILLBOARD_NEO, side});
        } else {
            // Árboles y postes
            float side = ((i / 8) % 2 == 0) ? -1.55f : 1.55f;
            PropType p = (i % 16 == 0) ? PropType::PALM_TREE : PropType::PINE_TREE;
            segments_[i].props.push_back({p, side});
            if (i % 16 == 0) {
                segments_[i].props.push_back({PropType::LAMPPOST, -side});
            }
        }
    }

    // Tráfico de coches rivales
    std::vector<SDL_Color> trafficColors = {
        {250, 204, 21, 255}, // Amarillo Neón
        {56, 189, 248, 255}, // Celeste Eléctrico
        {244, 63, 94, 255},  // Rosa Neón
        {168, 85, 247, 255}, // Púrpura Brutalista
        {255, 255, 255, 255} // Blanco Puro
    };

    int numCars = 18;
    for (int i = 0; i < numCars; ++i) {
        TrafficCar car;
        car.z = 250.0f + i * 420.0f;
        car.x = ((i % 3) - 1) * 0.55f;
        car.speed = 80.0f + (i % 5) * 15.0f;
        car.bodyColor = trafficColors[i % trafficColors.size()];
        traffic_.push_back(car);
    }
}

void Road::update(float dt) {
    if (dt <= 0.0f) dt = 0.016f;

    // Actualizar posición de los coches de tráfico
    for (auto& car : traffic_) {
        float deltaZ = (car.speed / 3.6f) * dt;
        car.z += deltaZ;
        if (car.z >= trackLength_) {
            car.z -= trackLength_;
        }
    }
}

const RoadSegment& Road::getSegment(int index) const {
    if (segments_.empty()) {
        static RoadSegment dummy;
        return dummy;
    }
    int idx = index % static_cast<int>(segments_.size());
    if (idx < 0) idx += segments_.size();
    return segments_[idx];
}

const RoadSegment& Road::findSegment(float z) const {
    int idx = static_cast<int>(std::floor(z / kSegmentLength));
    return getSegment(idx);
}
