#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <string>
#include "road.h"
#include "car.h"

class Renderer {
public:
    Renderer(SDL_Renderer* ren, TTF_Font* fontTitle, TTF_Font* fontHud, TTF_Font* fontSmall);
    ~Renderer();

    void renderGame(const Road& road, const Car& car, float raceTime, const Car* otherCar = nullptr, SDL_Color otherCarColor = {244, 63, 94, 255});
    void renderPlayerView(const Road& road, const Car& car, float raceTime, const SDL_Rect& viewport, bool isSplitScreen, int playerNum, const Car* otherCar = nullptr, SDL_Color otherCarColor = {244, 63, 94, 255});
    void renderSplitDivider(float p1Z, float p2Z, float trackLength);

    // Helpers neobrutalistas (100% compatibles con UI y otros módulos)
    void drawNeobrutalRect(int x, int y, int w, int h, SDL_Color fill, int borderW = 4, int shadowOffset = 6);
    void drawNeobrutalCircle(int cx, int cy, int radius, SDL_Color fill, int borderW = 4, int shadowOffset = 5);
    void drawText(TTF_Font* font, const std::string& text, int x, int y, SDL_Color color, bool center = false);
    void renderThickLine(float x1, float y1, float x2, float y2, float thickness, SDL_Color color);
    void renderTriangle(float x1, float y1, float x2, float y2, float x3, float y3, SDL_Color c1, SDL_Color c2, SDL_Color c3);

private:
    void renderSky(float carX, float skyScrollX, float horizonY, float raceTime, int viewW, int viewH);
    void renderRoad(const Road& road, const Car& car, float horizonY, float camTilt, float raceTime, int viewW, int viewH, const Car* otherCar, SDL_Color otherColor);
    void renderProp(const RoadProp& prop, int screenX, int screenY, float roadScreenW, float clipY, float raceTime, int segIndex, int viewW);
    void renderTrafficCar(const TrafficCar& car, int screenX, int screenY, float roadScreenW, float clipY, float segCurve, float raceTime);
    void renderSpeedEffects(const Car& car, float horizonY, float raceTime, int viewW, int viewH);
    void renderCockpit(const Car& car, const Road& road, float raceTime, int viewW, int viewH, bool isSplitScreen, const Car* otherCar, SDL_Color otherColor);
    void renderSteeringWheel(int cx, int cy, float angleDeg, float scale = 1.0f);

    // Primitivas nativas aceleradas por GPU (SDL_RenderGeometry)
    void renderPolygon(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, SDL_Color color);

    SDL_Renderer* ren_;
    TTF_Font* fontTitle_;
    TTF_Font* fontHud_;
    TTF_Font* fontSmall_;

    SDL_Texture* whiteTex_ = nullptr; // Textura 1x1 para dibujo acelerado

    // Variables de estado visual continuo para fluidez total
    float skyParallaxX_ = 0.0f;
    float smoothedCurve_ = 0.0f;
    float smoothedCameraX_ = 0.0f;

    static constexpr int kScreenWidth = 1920;
    static constexpr int kScreenHeight = 1080;
    static constexpr float kCameraHeight = 1050.0f; // Altura de cámara dentro de la cabina
    static constexpr float kCameraDepth = 0.84f;   // Campo de visión
    static constexpr int kDrawDistance = 280;       // Segmentos visibles adelante (horizonte profundo sin pop-in)
};
