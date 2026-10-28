#include "road.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

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
        seg.grassColor = {22, 163, 74, 255};   // Verde esmeralda profundo
        seg.rumbleColor = {239, 68, 68, 255};  // Rojo competición
        seg.roadColor = {28, 30, 38, 255};     // Asfalto oscuro frío
        seg.laneColor = {250, 250, 250, 255};  // Línea blanca reflectante
    } else {
        seg.grassColor = {52, 211, 102, 255};  // Verde lima vibrante
        seg.rumbleColor = {248, 250, 252, 255};// Blanco puro
        seg.roadColor = {39, 42, 52, 255};     // Asfalto medio
        seg.laneColor = {39, 42, 52, 255};     // Sin línea en alternancia
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
    if (endY < -99990.0f) {
        endY = startY; // Preservar altura constante por defecto
    }
    for (int i = 0; i < num; ++i) {
        float t = static_cast<float>(i) / num;
        float c = std::sin(t * 3.14159265f) * curve;
        float y = startY + (endY - startY) * (0.5f - 0.5f * std::cos(t * 3.14159265f));
        addSegment(c, y);
    }
}

void Road::addHill(int num, float height) {
    float startY = segments_.empty() ? 0.0f : segments_.back().worldY;
    for (int i = 0; i < num; ++i) {
        float t = static_cast<float>(i) / num;
        // Curva suave de campana usando (1 - cos(2*pi*t)) / 2 con pendiente 0 en ambos extremos
        float y = startY + (1.0f - std::cos(t * 2.0f * 3.14159265f)) * 0.5f * height;
        addSegment(0.0f, y);
    }
}

void Road::generateTrack(bool isTimeAttack) {
    segments_.clear();
    traffic_.clear();

    // =========================================================================
    // CIRCUITO DE COMPETICIÓN PROFESIONAL: CURVAS PRONUNCIADAS Y VARIADAS
    // Con horquillas cerradas, eses dobles, chicanes y cambios de elevación
    // =========================================================================

    // --- SECTOR 1: SECCIÓN DE ENTRADA Y SUBIDA TÉCNICA ---
    // 1. Recta de meta
    addStraight(8);

    // 2. Curva 1: Horquilla cerrada hacia la derecha (Frenada fuerte y giro cerrado)
    addCurve(60, 4.6f, 0.0f);
    addStraight(4);

    // 3. Curva 2: Horquilla enlazada hacia la izquierda
    addCurve(55, -4.8f, 0.0f);
    addStraight(5);

    // 4. Subida pronunciada con Curva amplia hacia la izquierda
    addHill(65, 420.0f);
    addCurve(70, -3.8f, 0.0f);
    addStraight(5);

    // 5. Curva en "S" técnica en bajada (Derecha rápida -> Izquierda violenta)
    addHill(65, -420.0f);
    addCurve(45, 4.5f, 0.0f);
    addStraight(3);
    addCurve(45, -4.8f, 0.0f);
    addStraight(4);

    // --- SECTOR 2: TRAMO DE ESES RÁPIDAS Y HORQUILLA DE MEDIA VELOCIDAD ---
    // 6. Recta intermedia de aceleración
    addStraight(8);

    // 7. Eses triples enlazadas (Izquierda -> Derecha -> Izquierda)
    addCurve(40, -4.2f, 0.0f);
    addStraight(2);
    addCurve(40, 4.6f, 0.0f);
    addStraight(2);
    addCurve(40, -4.4f, 0.0f);
    addStraight(4);

    // 8. Gran curvón peraltado a la derecha
    addCurve(75, 4.0f, 200.0f);
    addStraight(5);

    // 9. Doble vértice técnico a la izquierda (Requiere ceñirse a los pianos)
    addCurve(50, -5.2f, -200.0f);
    addStraight(4);
    addCurve(45, -4.6f, 0.0f);
    addStraight(4);

    // 10. Colina ciega (Crest) con curva cerrada a la derecha justo al coronar
    addHill(70, 460.0f);
    addCurve(55, 4.4f, -460.0f);
    addStraight(5);

    // --- SECTOR 3: CHICANES, HORQUILLAS Y RECTA TRASERA ---
    // 11. Recta larga de atrás (Zona de velocidad punta en 5ª y 6ª marcha)
    addStraight(20);

    // 12. Chicane "Bus Stop" (Frenada agresiva: Izquierda -> Derecha)
    addCurve(35, -5.5f, 0.0f);
    addStraight(3);
    addCurve(35, 5.5f, 0.0f);
    addStraight(4);

    // 13. Curva de radio decreciente a la derecha (Empieza media y se cierra)
    addCurve(40, 3.8f, 0.0f);
    addCurve(35, 5.2f, 0.0f);
    addStraight(4);

    // 14. Curva final amplia de entrada a la recta de meta
    addCurve(60, 4.2f, 0.0f);
    addStraight(8);

    trackLength_ = segments_.size() * kSegmentLength;

    // Decoración rica y variada a ambos lados de la pista (incluyendo rocas, postes y arboledas)
    // Arco de Meta / Salida en el segmento 0 y en el final
    if (!segments_.empty()) {
        segments_[0].props.push_back({PropType::FINISH_LINE_ARCH, 0.0f});
        segments_.back().props.push_back({PropType::FINISH_LINE_ARCH, 0.0f});
    }

    for (size_t i = 8; i < segments_.size(); i += 4) {
        if (i % 60 == 0) {
            // Arcos de Checkpoint solo en modo arcade multijugador (en Time Attack no se usan)
            if (!isTimeAttack) {
                segments_[i].props.push_back({PropType::CHECKPOINT_ARCH, 0.0f});
                segments_[i].isCheckpoint = true;
            }
        } else if (i % 24 == 0) {
            // Carteles Neobrutalistas + rocas decorativas en el lado opuesto
            float side = ((i / 24) % 2 == 0) ? -1.75f : 1.75f;
            segments_[i].props.push_back({PropType::BILLBOARD_NEO, side});
            segments_[i].props.push_back({PropType::ROCK, -side * 1.15f});
        } else if (i % 8 == 0) {
            // Árboles principales y farolas en el borde del camino
            float side = ((i / 8) % 2 == 0) ? -1.58f : 1.58f;
            PropType treeType = (i % 16 == 0) ? PropType::PALM_TREE : PropType::PINE_TREE;
            segments_[i].props.push_back({treeType, side});

            if (i % 16 == 0) {
                segments_[i].props.push_back({PropType::LAMPPOST, -side * 0.85f});
                // Segunda hilera de bosque profundo para dar volumen lateral
                segments_[i].props.push_back({PropType::PINE_TREE, side * 1.55f});
            } else {
                segments_[i].props.push_back({PropType::ROCK, -side * 1.18f});
            }
        } else {
            // Vegetación y rocas secundarias escalonadas
            float outerSide = ((i / 4) % 2 == 0) ? -2.25f : 2.25f;
            PropType bgProp = (i % 12 == 0) ? PropType::PALM_TREE : PropType::PINE_TREE;
            segments_[i].props.push_back({bgProp, outerSide});
        }
    }

    // Tráfico de coches rivales distribuidos en los 3 carriles
    std::vector<SDL_Color> trafficColors = {
        {250, 204, 21, 255}, // Amarillo Neón
        {56, 189, 248, 255}, // Celeste Eléctrico
        {244, 63, 94, 255},  // Rosa Neón
        {168, 85, 247, 255}, // Púrpura Brutalista
        {255, 255, 255, 255},// Blanco Puro
        {34, 197, 94, 255}   // Verde Racing
    };

    const float laneOffsets[3] = {-0.66f, 0.0f, 0.66f};

    int numCars = 20;
    for (int i = 0; i < numCars; ++i) {
        TrafficCar car;
        car.z = 120.0f + i * 160.0f;
        car.x = laneOffsets[i % 3];
        car.speed = 85.0f + (i % 4) * 16.0f;
        car.bodyColor = trafficColors[i % trafficColors.size()];
        traffic_.push_back(car);
    }
}

void Road::enableTraffic(bool enable) {
    traffic_.clear();
    if (!enable) return;

    std::vector<SDL_Color> trafficColors = {
        {250, 204, 21, 255}, // Amarillo Neón
        {56, 189, 248, 255}, // Celeste Eléctrico
        {244, 63, 94, 255},  // Rosa Neón
        {168, 85, 247, 255}, // Púrpura Brutalista
        {255, 255, 255, 255},// Blanco Puro
        {34, 197, 94, 255}   // Verde Racing
    };
    const float laneOffsets[3] = {-0.66f, 0.0f, 0.66f};
    int numCars = 20;
    for (int i = 0; i < numCars; ++i) {
        TrafficCar car;
        car.z = 120.0f + i * 160.0f;
        car.x = laneOffsets[i % 3];
        car.speed = 85.0f + (i % 4) * 16.0f;
        car.bodyColor = trafficColors[i % trafficColors.size()];
        traffic_.push_back(car);
    }
}

void Road::update(float dt) {
    if (dt <= 0.0f) dt = 0.016f;
    dt = std::min(dt, 0.05f);

    const float laneOffsets[3] = {-0.66f, 0.0f, 0.66f};

    // Actualizar posición de los coches de tráfico con micro-oscilación orgánica de carril
    for (size_t i = 0; i < traffic_.size(); ++i) {
        auto& car = traffic_[i];
        float deltaZ = (car.speed / 3.6f) * dt;
        car.z += deltaZ;
        if (car.z >= trackLength_) {
            car.z -= trackLength_;
        }

        // Mantener el carril base con un suave y sutil serpenteo de autopista muy fluido
        float baseLane = laneOffsets[i % 3];
        float weave = std::sin(car.z * 0.008f + static_cast<float>(i) * 1.3f) * 0.025f;
        car.x = baseLane + weave;
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
