#pragma once

#include <vector>
#include <SDL.h>

enum class PropType {
    NONE,
    PALM_TREE,
    PINE_TREE,
    BILLBOARD_NEO,
    CHECKPOINT_ARCH,
    LAMPPOST,
    ROCK
};

struct RoadProp {
    PropType type = PropType::NONE;
    float offset = 0.0f; // -1.8 (izq) a +1.8 (der)
};

struct TrafficCar {
    float x = 0.0f;       // -0.8 a +0.8
    float z = 0.0f;       // metros
    float speed = 80.0f;  // km/h
    SDL_Color bodyColor;
};

struct RoadSegment {
    int index = 0;
    float worldX = 0.0f;
    float worldY = 0.0f;
    float worldZ = 0.0f;

    float curve = 0.0f; // Curvatura horizontal
    float clipY = 1080.0f;

    // Colores alternados estilo neobrutalista
    SDL_Color grassColor;
    SDL_Color rumbleColor;
    SDL_Color roadColor;
    SDL_Color laneColor;

    std::vector<RoadProp> props;
    bool isCheckpoint = false;
};

class Road {
public:
    Road();

    void generateTrack();
    void update(float dt);

    const RoadSegment& getSegment(int index) const;
    const RoadSegment& findSegment(float z) const;
    int getSegmentCount() const { return static_cast<int>(segments_.size()); }
    float getTrackLength() const { return trackLength_; }

    const std::vector<TrafficCar>& getTraffic() const { return traffic_; }
    std::vector<TrafficCar>& getTraffic() { return traffic_; }

    static constexpr float kSegmentLength = 12.0f; // metros por segmento
    static constexpr float kRoadWidth = 1400.0f;   // ancho 3D

private:
    void addSegment(float curve, float y);
    void addStraight(int num);
    void addCurve(int num, float curve, float endY = 0.0f);
    void addHill(int num, float height);

    std::vector<RoadSegment> segments_;
    std::vector<TrafficCar> traffic_;
    float trackLength_ = 0.0f;
};
