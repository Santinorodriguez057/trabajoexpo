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

    void renderGame(const Road& road, const Car& car, float raceTime);

    // Helpers neobrutalistas
    void drawNeobrutalRect(int x, int y, int w, int h, SDL_Color fill, int borderW = 4, int shadowOffset = 6);
    void drawNeobrutalCircle(int cx, int cy, int radius, SDL_Color fill, int borderW = 4, int shadowOffset = 5);
    void drawText(TTF_Font* font, const std::string& text, int x, int y, SDL_Color color, bool center = false);

private:
    void renderSky(float carX, float curve);
    void renderRoad(const Road& road, const Car& car);
    void renderProp(const RoadProp& prop, int screenX, int screenY, float scale);
    void renderTrafficCar(const TrafficCar& car, int screenX, int screenY, float scale);
    void renderCockpit(const Car& car);
    void renderSteeringWheel(int cx, int cy, float angleDeg);
    void renderPolygon(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, SDL_Color color);

    SDL_Renderer* ren_;
    TTF_Font* fontTitle_;
    TTF_Font* fontHud_;
    TTF_Font* fontSmall_;

    SDL_Texture* whiteTex_ = nullptr; // Textura 1x1 para dibujo acelerado

    static constexpr int kScreenWidth = 1920;
    static constexpr int kScreenHeight = 1080;
    static constexpr float kCameraHeight = 1050.0f; // Altura de cámara dentro de la cabina
    static constexpr float kCameraDepth = 0.84f;   // Campo de visión
    static constexpr int kDrawDistance = 140;       // Segmentos visibles adelante
};
