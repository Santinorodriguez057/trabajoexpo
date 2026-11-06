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

// ---------------------------------------------------------------------------
// Efectos visuales (todos planos, con borde negro: estética neobrutalista)
// ---------------------------------------------------------------------------
struct SparkParticle {
    float x = 0.0f, y = 0.0f;
    float vx = 0.0f, vy = 0.0f;
    float size = 8.0f;
    float life = 1.0f;
    float rot = 0.0f, vrot = 0.0f;
    SDL_Color color{255, 255, 255, 255};
};

struct ShockRing {               // Onda expansiva gruesa al cortar
    float x = 0.0f, y = 0.0f;
    float radius = 10.0f, maxRadius = 120.0f;
    float thickness = 12.0f;
    float life = 1.0f;
    SDL_Color color{255, 255, 255, 255};
};

struct SlashFlash {              // Línea de tajo recta que atraviesa al objeto
    float x = 0.0f, y = 0.0f;
    float angle = 0.0f;
    float length = 200.0f;
    float life = 1.0f;
    SDL_Color color{255, 255, 255, 255};
};

// ---------------------------------------------------------------------------
// Datos que el juego pasa a cada pantalla
// ---------------------------------------------------------------------------
struct HudSoloData {
    int displayScore = 0;
    int best = 0;
    int combo = 0;
    float comboFrac = 0.0f;
    int lives = 3;
    float scorePop = 0.0f;
    float lifeShake = 0.0f;
    float time = 0.0f;
};

struct Hud1v1Data {
    int score[2] = {0, 0};
    int displayScore[2] = {0, 0};
    int combo[2] = {0, 0};
    float comboFrac[2] = {0.0f, 0.0f};
    float scorePop[2] = {0.0f, 0.0f};
    float timeLeft = 60.0f;
    float time = 0.0f;
};

struct MenuData {
    float time = 0.0f;
    float anim = 0.0f;           // 0..1 animación de entrada
    int selected = 0;            // 0 = Solo, 1 = Versus
    bool versusUnlocked = false;
    bool p1Connected = false;
    bool p1IsArcade = false;
    bool p2Connected = false;
    int best = 0;
};

struct GameOverData {
    float time = 0.0f;
    float anim = 0.0f;           // segundos desde que terminó la partida
    bool versus = false;
    int score[2] = {0, 0};
    int displayScore[2] = {0, 0};
    int cuts[2] = {0, 0};
    int perfects[2] = {0, 0};
    int maxCombo[2] = {0, 0};
    int best = 0;
    bool newRecord = false;
    char rank = 'C';
    float rankReveal = 0.0f;     // 0..1
};

class GameRenderer {
public:
    GameRenderer(SDL_Renderer* ren, TTF_Font* fontGiant, TTF_Font* fontTitleBig,
                 TTF_Font* fontTitle, TTF_Font* fontHud, TTF_Font* fontSmall);
    ~GameRenderer();

    // Ciclo de frame: el "mundo" se dibuja en una textura para poder sacudir la cámara
    void beginFrame();
    void beginWorld();
    void endWorld(float shakeX, float shakeY);
    void fillScreen(SDL_Color c);

    // Mundo
    void renderBackground(float time, int accentLevel, bool danger);
    void renderEntities(const std::vector<CyberEntity>& entities, float time);
    void renderHalves(const std::vector<CutHalf>& halves);
    void renderParticles(const std::vector<SparkParticle>& particles);
    void renderRings(const std::vector<ShockRing>& rings);
    void renderSlashFlashes(const std::vector<SlashFlash>& flashes);
    void renderTrails(const std::vector<SlashTrailPoint>& trail, SDL_Color col,
                      bool fingerDown, float fx, float fy);
    void renderFloatingScores(const std::vector<FloatingScore>& scores);

    // Pantallas / HUD (sin sacudida)
    void renderHudSolo(const HudSoloData& d);
    void renderHud1v1(const Hud1v1Data& d);
    void renderMenu(const MenuData& d);
    void renderCountdown(int number, float beatT);
    void renderGameOver(const GameOverData& d);

    // Primitivas neobrutalistas
    void drawNeobrutalRect(int x, int y, int w, int h, SDL_Color fill, int borderW = 4, int shadowOffset = 6);
    void drawNeobrutalCircle(int cx, int cy, int radius, SDL_Color fill, int borderW = 4, int shadowOffset = 6);
    void drawText(TTF_Font* font, const std::string& text, int x, int y, SDL_Color color,
                  bool center = false, bool shadow = true);
    void drawTextScaled(TTF_Font* font, const std::string& text, float cx, float cy, SDL_Color color,
                        float scale = 1.0f, float angleDeg = 0.0f, int outline = 0);
    void drawStampBox(float cx, float cy, float w, float h, float angleDeg, SDL_Color fill,
                      int border = 4, int shadow = 6);
    void drawStamp(TTF_Font* font, const std::string& text, float cx, float cy, float scale,
                   float angleDeg, SDL_Color fill, SDL_Color textCol);
    void drawRing(float cx, float cy, float radius, float thickness, SDL_Color color);
    void renderThickLine(float x1, float y1, float x2, float y2, float thickness, SDL_Color color);
    void fillTriangle(float x1, float y1, float x2, float y2, float x3, float y3, SDL_Color c);
    void drawHazardBand(int x, int y, int w, int h, float offset, SDL_Color a, SDL_Color b);

    // Ícono de objeto (también usado en el tutorial del menú)
    void drawEntity(EntityType type, float cx, float cy, float radius, float angle, float idealAngle, float time);

private:
    struct TextTex {
        SDL_Texture* tex = nullptr;
        int w = 0;
        int h = 0;
        Uint32 lastUsed = 0;
    };
    const TextTex* getText(TTF_Font* font, const std::string& text);
    void drawQuad(float cx, float cy, float w, float h, float angleDeg, SDL_Color c);
    void drawHalfDisc(float cx, float cy, float r, float startAngle, SDL_Color c);
    void drawTitleBounce(const std::string& text, float cx, float cy, float time, float anim);
    void drawPadlock(int cx, int cy);
    void drawTugBar(int x, int y, int w, int h, int s1, int s2);

    SDL_Renderer* ren_ = nullptr;
    TTF_Font* fontGiant_ = nullptr;
    TTF_Font* fontTitleBig_ = nullptr;
    TTF_Font* fontTitle_ = nullptr;
    TTF_Font* fontHud_ = nullptr;
    TTF_Font* fontSmall_ = nullptr;

    SDL_Texture* worldTex_ = nullptr;
    std::unordered_map<std::string, TextTex> textCache_;
    Uint32 frame_ = 0;

    std::vector<SDL_Vertex> verts_;
    std::vector<int> idx_;
};
