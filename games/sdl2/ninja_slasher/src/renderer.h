#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <vector>
#include <string>
#include <unordered_map>
#include "entity.h"
#include "input.h"
#include "score.h"
#include "theme.h"

struct SparkParticle {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float size = 8.0f;
    float life = 1.0f;
    SDL_Color color;
};

class GameRenderer {
public:
    GameRenderer(SDL_Renderer* ren, TTF_Font* fontTitle, TTF_Font* fontHud, TTF_Font* fontSmall);
    ~GameRenderer();

    void renderBackground(int w, int h, float time);
    void renderEntities(const std::vector<CyberEntity>& entities);
    void renderHalves(const std::vector<CutHalf>& halves);
    void renderParticles(const std::vector<SparkParticle>& particles);
    void renderTrails(const std::vector<SlashTrailPoint>& trail, SDL_Color col);
    void renderHudSolo(int score, int combo, int lives);
    void renderHud1v1(int p1Score, int p1Combo, int p2Score, int p2Combo, float timeLeft);
    void renderFloatingScores(const std::vector<FloatingScore>& scores);

    // Primitivas Neobrutalistas
    void drawNeobrutalRect(int x, int y, int w, int h, SDL_Color fill, int borderW = 4, int shadowOffset = 6);
    void drawNeobrutalCircle(int cx, int cy, int radius, SDL_Color fill, int borderW = 4, int shadowOffset = 6);
    void drawText(TTF_Font* font, const std::string& text, int x, int y, SDL_Color color, bool center = false);
    void renderThickLine(float x1, float y1, float x2, float y2, float thickness, SDL_Color color);

private:
    SDL_Renderer* ren_ = nullptr;
    TTF_Font* fontTitle_ = nullptr;
    TTF_Font* fontHud_ = nullptr;
    TTF_Font* fontSmall_ = nullptr;

    SDL_Texture* whiteTex_ = nullptr;

    struct TextCacheEntry {
        SDL_Texture* tex = nullptr;
        int w = 0;
        int h = 0;
    };
    std::unordered_map<std::string, TextCacheEntry> textCache_;

    void drawEnergyCore(float cx, float cy, float radius, float angle);
    void drawCyberDrone(float cx, float cy, float radius, float angle, float idealAngle);
    void drawMechaShuriken(float cx, float cy, float radius, float angle);
    void drawEmpMine(float cx, float cy, float radius, float angle);
};

