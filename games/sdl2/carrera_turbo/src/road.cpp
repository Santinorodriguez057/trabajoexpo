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
        // Factor global para curvas más pronunciadas (afecta visual, física y minimapa por igual)
        const float kCurveBoost = 1.3f;
        float c = std::sin(t * 3.14159265f) * curve * kCurveBoost;
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

void Road::generateTrack(bool isTimeAttack, bool enableNitroAndMud) {
    segments_.clear();
    traffic_.clear();

    // =========================================================================
    // CIRCUITO DE COMPETICIÓN PROFESIONAL - FORMATO EXPO (~1:15 min por vuelta)
    // Trazado de Gran Premio limpio y dinámico con 4 sectores de frenada, curvas técnicas y chicanes
    // =========================================================================

    // --- SECTOR 1: SALIDA, CURVA 1 Y ESES TÉCNICAS (HACIA EL ESTE) ---
    // 1. Recta principal de meta y parrilla de salida
    addStraight(22);

    // 2. Curva 1: Frenada fuerte a derechas (90 grados)
    addCurve(16, 4.6f, 0.0f);

    // 3. Recta de aceleración hacia las eses
    addStraight(20);

    // 4. Eses técnicas enlazadas (Izquierda -> Derecha)
    addCurve(12, -4.2f, 0.0f);
    addCurve(12, 4.2f, 0.0f);

    // 5. Subida a la colina con pianos altos
    addStraight(20);

    // --- SECTOR 2: CURVÓN EN BAJADA, RECTA INTERMEDIA Y HORQUILLA (HACIA EL SUR/OESTE) ---
    // 6. Curva 2: Gran curvón peraltado a derechas (90 grados) con descenso
    addHill(18, 180.0f);
    addCurve(18, 4.4f, -180.0f);

    // 7. Recta intermedia de alta velocidad
    addStraight(35);

    // 8. Curva 3: Horquilla técnica a derechas (90 grados)
    addCurve(16, 5.0f, 0.0f);

    // --- SECTOR 3: RECTA TRASERA, CHICANE 'BUS STOP' Y PARABÓLICA FINAL (HACIA EL NORTE) ---
    // 9. Recta trasera
    addStraight(14);

    // 10. Chicane 'Bus Stop' de gran precisión (Izquierda -> Derecha)
    addCurve(12, -4.8f, 0.0f);
    addCurve(12, 4.8f, 0.0f);

    // 11. Recta de aproximación a la parabólica final
    addStraight(26);

    // 12. Curva 4: Parabólica final a derechas (90 grados) de vuelta a meta
    addCurve(18, 4.6f, 0.0f);

    // 13. Recta final hacia la línea de llegada
    addStraight(14);

    trackLength_ = segments_.size() * kSegmentLength;

    // =========================================================================
    // INTEGRACIÓN CONTINUA DE COORDENADAS 3D DE LA PISTA Y SILUETA 2D DEL MINIMAPA
    // Garantiza que el minimapa coincida 1:1 exactamente con las curvas que ve y toma el jugador
    // =========================================================================
    int totalSegs = static_cast<int>(segments_.size());
    float curHeading = 0.0f;
    float curX = 0.0f;
    const float kCurveIntegrationFactor = 0.0075f;

    // 1. Suma total de curvatura para cerrar en un bucle cerrado continuo de 360 grados (2 * PI)
    float sumCurve = 0.0f;
    for (int i = 0; i < totalSegs; ++i) {
        sumCurve += segments_[i].curve;
    }
    const float kPi2 = 6.2831853f;
    float mapAngleScale = (std::abs(sumCurve) > 0.01f) ? (kPi2 / sumCurve) : 0.024f;

    // 2. Integrar odometría 2D exacta (X, Y)
    std::vector<std::pair<float, float>> raw2D(totalSegs + 1, {0.0f, 0.0f});
    float mapHeading = 0.0f;
    for (int i = 0; i < totalSegs; ++i) {
        segments_[i].heading = curHeading;
        segments_[i].worldX = curX;

        curHeading += segments_[i].curve * kCurveIntegrationFactor;
        curX += std::sin(curHeading) * kSegmentLength * 720.0f;

        mapHeading += segments_[i].curve * mapAngleScale;
        float nx = raw2D[i].first + std::sin(mapHeading) * kSegmentLength;
        float ny = raw2D[i].second - std::cos(mapHeading) * kSegmentLength;
        raw2D[i + 1] = {nx, ny};
    }

    // 3. Compensar suavemente la pequeña discrepancia residual de cierre para un bucle 2D perfecto
    float gapX = raw2D.back().first;
    float gapY = raw2D.back().second;
    for (int i = 0; i < totalSegs; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(totalSegs);
        segments_[i].mapX = raw2D[i].first - gapX * t;
        segments_[i].mapY = raw2D[i].second - gapY * t;
    }

    // Decoración rica y variada a ambos lados de la pista (incluyendo rocas, postes y arboledas)
    // Arco de Meta / Salida único en la línea de largada (segmento 0)
    if (!segments_.empty()) {
        segments_[0].props.push_back({PropType::FINISH_LINE_ARCH, 0.0f});
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

    // BOTES DE NITRO Y CHARCOS DE BARRO SOBRE LA PISTA (offset entre -0.7 y +0.7)
    // Solo se generan si están habilitadas las mecánicas avanzadas (Dificultad Difícil)
    if (enableNitroAndMud) {
        for (size_t i = 15; i < segments_.size() - 15; ++i) {
            if (i % 38 == 0) {
                // Botellas de Nitro en carriles izquierdo, centro o derecho
                float nitroOffsets[3] = {-0.55f, 0.0f, 0.55f};
                float off = nitroOffsets[(i / 38) % 3];
                segments_[i].props.push_back({PropType::NITRO_BOTTLE, off});
            } else if (i % 45 == 22) {
                // Charcos de barro técnico
                float mudOffsets[3] = {0.45f, -0.45f, 0.0f};
                float off = mudOffsets[(i / 45) % 3];
                segments_[i].props.push_back({PropType::MUD_PUDDLE, off});
            }
        }
    }

    // TRIBUNAS CON PÚBLICO POR SECTORES (recta de meta, chicane y horquilla final)
    auto addStands = [&](int from, int to, float side) {
        for (int i = std::max(0, from); i < to && i < static_cast<int>(segments_.size()); ++i) {
            auto& props = segments_[i].props;
            props.erase(std::remove_if(props.begin(), props.end(), [&](const RoadProp& p) {
                return p.type != PropType::FINISH_LINE_ARCH && p.type != PropType::CHECKPOINT_ARCH && (p.offset * side) > 0.0f;
            }), props.end());
            if ((i - from) % 5 == 0) props.push_back({PropType::GRANDSTAND, side * 2.05f});
        }
    };
    int nTotal = static_cast<int>(segments_.size());
    addStands(2, 14, -1.0f);                  // Recta de meta (izquierda)
    addStands(4, 12, 1.0f);                   // Recta de meta (derecha)
    int sector3 = (nTotal * 2) / 3;
    addStands(sector3 + 12, sector3 + 28, 1.0f); // Zona de chicane (Sector 3)
    addStands(nTotal - 28, nTotal - 8, -1.0f);  // Horquilla final y recta previa a meta

    // Tráfico de coches desactivado para pista limpia
    traffic_.clear();
}

void Road::enableTraffic(bool enable) {
    traffic_.clear();
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

float Road::getInterpolatedCurve(float z) const {
    if (segments_.empty()) return 0.0f;
    float segPos = z / kSegmentLength;
    int idx0 = static_cast<int>(std::floor(segPos));
    float t = segPos - idx0;
    float c0 = getSegment(idx0).curve;
    float c1 = getSegment(idx0 + 1).curve;
    return c0 + (c1 - c0) * t;
}
