#include "renderer.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace {
    constexpr float kPi = 3.1415926535f;
    constexpr int kW = 1920;
    constexpr int kH = 1080;

    float clamp01(float t) { return std::clamp(t, 0.0f, 1.0f); }
    float easeOutBack(float t) {
        t = clamp01(t);
        const float c1 = 1.70158f, c3 = c1 + 1.0f;
        return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
    }
    float easeOutBounce(float t) {
        t = clamp01(t);
        const float n1 = 7.5625f, d1 = 2.75f;
        if (t < 1.0f / d1) return n1 * t * t;
        if (t < 2.0f / d1) { t -= 1.5f / d1; return n1 * t * t + 0.75f; }
        if (t < 2.5f / d1) { t -= 2.25f / d1; return n1 * t * t + 0.9375f; }
        t -= 2.625f / d1;
        return n1 * t * t + 0.984375f;
    }
    bool blink(float time, float hz) { return static_cast<int>(time * hz) % 2 == 0; }

    SDL_Color comboTierColor(int combo) {
        if (combo >= 6) return CyberTheme::cP2Pink;
        if (combo >= 4) return CyberTheme::cOrange;
        return CyberTheme::cYellow;
    }
    const char* comboTierLabel(int combo) {
        if (combo >= 6) return "¡FRENESÍ!";
        if (combo >= 4) return "¡FURIA!";
        return "COMBO";
    }
    SDL_Color rankColor(char r) {
        switch (r) {
            case 'S': return CyberTheme::cP2Pink;
            case 'A': return CyberTheme::cYellow;
            case 'B': return CyberTheme::cP1Cyan;
            case 'C': return CyberTheme::cGreen;
            default:  return CyberTheme::cSilver;
        }
    }
}

// ===========================================================================
// Construcción / ciclo de frame
// ===========================================================================
GameRenderer::GameRenderer(SDL_Renderer* ren, TTF_Font* fontGiant, TTF_Font* fontTitleBig,
                           TTF_Font* fontTitle, TTF_Font* fontHud, TTF_Font* fontSmall)
    : ren_(ren), fontGiant_(fontGiant), fontTitleBig_(fontTitleBig),
      fontTitle_(fontTitle), fontHud_(fontHud), fontSmall_(fontSmall) {
    if (SDL_RenderTargetSupported(ren_)) {
        worldTex_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, kW, kH);
    }
    verts_.reserve(256);
    idx_.reserve(512);
}

GameRenderer::~GameRenderer() {
    for (auto& kv : textCache_) {
        if (kv.second.tex) SDL_DestroyTexture(kv.second.tex);
    }
    textCache_.clear();
    if (worldTex_) SDL_DestroyTexture(worldTex_);
}

void GameRenderer::beginFrame() {
    ++frame_;
    // Expulsar del caché los textos que no se usan desde hace un par de frames
    if (textCache_.size() > 300) {
        for (auto it = textCache_.begin(); it != textCache_.end();) {
            if (it->second.lastUsed + 2 < frame_) {
                if (it->second.tex) SDL_DestroyTexture(it->second.tex);
                it = textCache_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void GameRenderer::beginWorld() {
    if (worldTex_) SDL_SetRenderTarget(ren_, worldTex_);
}

void GameRenderer::endWorld(float shakeX, float shakeY) {
    if (!worldTex_) return;
    SDL_SetRenderTarget(ren_, nullptr);
    SDL_SetRenderDrawColor(ren_, CyberTheme::cDarkBg.r, CyberTheme::cDarkBg.g, CyberTheme::cDarkBg.b, 255);
    SDL_RenderClear(ren_);
    SDL_Rect dst = {static_cast<int>(shakeX), static_cast<int>(shakeY), kW, kH};
    SDL_RenderCopy(ren_, worldTex_, nullptr, &dst);
}

void GameRenderer::fillScreen(SDL_Color c) {
    SDL_SetRenderDrawColor(ren_, c.r, c.g, c.b, 255);
    SDL_Rect r = {0, 0, kW, kH};
    SDL_RenderFillRect(ren_, &r);
}

// ===========================================================================
// Primitivas
// ===========================================================================
void GameRenderer::drawNeobrutalRect(int x, int y, int w, int h, SDL_Color fill, int borderW, int shadowOffset) {
    if (w <= 0 || h <= 0) return;
    const SDL_Color& k = CyberTheme::cBlack;
    if (shadowOffset > 0) {
        SDL_Rect sR = {x - borderW + shadowOffset, y - borderW + shadowOffset, w + borderW * 2, h + borderW * 2};
        SDL_SetRenderDrawColor(ren_, k.r, k.g, k.b, 255);
        SDL_RenderFillRect(ren_, &sR);
    }
    if (borderW > 0) {
        SDL_Rect bR = {x - borderW, y - borderW, w + borderW * 2, h + borderW * 2};
        SDL_SetRenderDrawColor(ren_, k.r, k.g, k.b, 255);
        SDL_RenderFillRect(ren_, &bR);
    }
    SDL_Rect mR = {x, y, w, h};
    SDL_SetRenderDrawColor(ren_, fill.r, fill.g, fill.b, 255);
    SDL_RenderFillRect(ren_, &mR);
}

void GameRenderer::drawNeobrutalCircle(int cx, int cy, int radius, SDL_Color fill, int borderW, int shadowOffset) {
    if (radius <= 0) return;
    auto fillCircle = [&](float px, float py, float r, SDL_Color col) {
        const int segs = std::clamp(static_cast<int>(r / 2.0f) + 16, 18, 56);
        verts_.resize(segs + 1);
        idx_.resize(segs * 3);
        verts_[0] = {{px, py}, col, {0, 0}};
        for (int i = 0; i < segs; ++i) {
            float a = (static_cast<float>(i) / segs) * 2.0f * kPi;
            verts_[i + 1] = {{px + std::cos(a) * r, py + std::sin(a) * r}, col, {0, 0}};
            idx_[i * 3 + 0] = 0;
            idx_[i * 3 + 1] = i + 1;
            idx_[i * 3 + 2] = (i + 1 == segs) ? 1 : (i + 2);
        }
        SDL_RenderGeometry(ren_, nullptr, verts_.data(), static_cast<int>(verts_.size()),
                           idx_.data(), static_cast<int>(idx_.size()));
    };
    if (shadowOffset > 0) fillCircle(cx + shadowOffset, cy + shadowOffset, radius + borderW, CyberTheme::cBlack);
    if (borderW > 0) fillCircle(cx, cy, radius + borderW, CyberTheme::cBlack);
    fillCircle(cx, cy, radius, fill);
}

void GameRenderer::drawRing(float cx, float cy, float radius, float thickness, SDL_Color color) {
    if (thickness < 0.5f || radius <= 0.0f) return;
    const int segs = 48;
    float rIn = std::max(0.0f, radius - thickness * 0.5f);
    float rOut = radius + thickness * 0.5f;
    verts_.resize(segs * 2);
    idx_.resize(segs * 6);
    for (int i = 0; i < segs; ++i) {
        float a = (static_cast<float>(i) / segs) * 2.0f * kPi;
        float c = std::cos(a), s = std::sin(a);
        verts_[i * 2 + 0] = {{cx + c * rOut, cy + s * rOut}, color, {0, 0}};
        verts_[i * 2 + 1] = {{cx + c * rIn, cy + s * rIn}, color, {0, 0}};
        int n = (i + 1) % segs;
        idx_[i * 6 + 0] = i * 2;     idx_[i * 6 + 1] = n * 2;     idx_[i * 6 + 2] = i * 2 + 1;
        idx_[i * 6 + 3] = i * 2 + 1; idx_[i * 6 + 4] = n * 2;     idx_[i * 6 + 5] = n * 2 + 1;
    }
    SDL_RenderGeometry(ren_, nullptr, verts_.data(), static_cast<int>(verts_.size()),
                       idx_.data(), static_cast<int>(idx_.size()));
}

void GameRenderer::renderThickLine(float x1, float y1, float x2, float y2, float thickness, SDL_Color color) {
    float dx = x2 - x1, dy = y2 - y1;
    float len = std::hypot(dx, dy);
    if (len < 0.001f || thickness <= 0.0f) return;
    float nx = (-dy / len) * (thickness * 0.5f);
    float ny = ( dx / len) * (thickness * 0.5f);
    SDL_Vertex v[4] = {
        {{x1 + nx, y1 + ny}, color, {0, 0}}, {{x2 + nx, y2 + ny}, color, {0, 0}},
        {{x2 - nx, y2 - ny}, color, {0, 0}}, {{x1 - nx, y1 - ny}, color, {0, 0}}};
    int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(ren_, nullptr, v, 4, idx, 6);
}

void GameRenderer::fillTriangle(float x1, float y1, float x2, float y2, float x3, float y3, SDL_Color c) {
    SDL_Vertex v[3] = {{{x1, y1}, c, {0, 0}}, {{x2, y2}, c, {0, 0}}, {{x3, y3}, c, {0, 0}}};
    SDL_RenderGeometry(ren_, nullptr, v, 3, nullptr, 0);
}

void GameRenderer::drawQuad(float cx, float cy, float w, float h, float angleDeg, SDL_Color c) {
    float a = angleDeg * kPi / 180.0f;
    float ca = std::cos(a), sa = std::sin(a);
    float hw = w * 0.5f, hh = h * 0.5f;
    const float px[4] = {-hw, hw, hw, -hw};
    const float py[4] = {-hh, -hh, hh, hh};
    SDL_Vertex v[4];
    for (int i = 0; i < 4; ++i) {
        v[i] = {{cx + px[i] * ca - py[i] * sa, cy + px[i] * sa + py[i] * ca}, c, {0, 0}};
    }
    int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(ren_, nullptr, v, 4, idx, 6);
}

void GameRenderer::drawStampBox(float cx, float cy, float w, float h, float angleDeg, SDL_Color fill, int border, int shadow) {
    if (shadow > 0) drawQuad(cx + shadow, cy + shadow, w + border * 2, h + border * 2, angleDeg, CyberTheme::cBlack);
    if (border > 0) drawQuad(cx, cy, w + border * 2, h + border * 2, angleDeg, CyberTheme::cBlack);
    drawQuad(cx, cy, w, h, angleDeg, fill);
}

void GameRenderer::drawHalfDisc(float cx, float cy, float r, float startAngle, SDL_Color c) {
    const int segs = 20;
    verts_.resize(segs + 2);
    idx_.resize(segs * 3);
    verts_[0] = {{cx, cy}, c, {0, 0}};
    for (int i = 0; i <= segs; ++i) {
        float a = startAngle + (static_cast<float>(i) / segs) * kPi;
        verts_[i + 1] = {{cx + std::cos(a) * r, cy + std::sin(a) * r}, c, {0, 0}};
    }
    for (int i = 0; i < segs; ++i) {
        idx_[i * 3 + 0] = 0;
        idx_[i * 3 + 1] = i + 1;
        idx_[i * 3 + 2] = i + 2;
    }
    SDL_RenderGeometry(ren_, nullptr, verts_.data(), static_cast<int>(verts_.size()),
                       idx_.data(), static_cast<int>(idx_.size()));
}

void GameRenderer::drawHazardBand(int x, int y, int w, int h, float offset, SDL_Color a, SDL_Color b) {
    SDL_Rect clip = {x, y, w, h};
    SDL_RenderSetClipRect(ren_, &clip);
    SDL_SetRenderDrawColor(ren_, a.r, a.g, a.b, 255);
    SDL_RenderFillRect(ren_, &clip);
    const float stripe = static_cast<float>(h) * 1.4f;
    float start = std::fmod(offset, stripe * 2.0f) - stripe * 2.0f;
    for (float sx = x + start; sx < x + w + h; sx += stripe * 2.0f) {
        SDL_Vertex v[4] = {
            {{sx, static_cast<float>(y + h)}, b, {0, 0}},
            {{sx + h, static_cast<float>(y)}, b, {0, 0}},
            {{sx + h + stripe, static_cast<float>(y)}, b, {0, 0}},
            {{sx + stripe, static_cast<float>(y + h)}, b, {0, 0}}};
        int idx[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(ren_, nullptr, v, 4, idx, 6);
    }
    SDL_RenderSetClipRect(ren_, nullptr);
    SDL_SetRenderDrawColor(ren_, 18, 18, 18, 255);
    SDL_Rect top = {x, y, w, 4};
    SDL_Rect bot = {x, y + h - 4, w, 4};
    SDL_RenderFillRect(ren_, &top);
    SDL_RenderFillRect(ren_, &bot);
}

// ===========================================================================
// Texto (caché de texturas blancas teñidas con color-mod)
// ===========================================================================
const GameRenderer::TextTex* GameRenderer::getText(TTF_Font* font, const std::string& text) {
    if (!font || text.empty()) return nullptr;
    char prefix[32];
    std::snprintf(prefix, sizeof(prefix), "%p|", static_cast<void*>(font));
    std::string key = std::string(prefix) + text;

    auto it = textCache_.find(key);
    if (it == textCache_.end()) {
        TextTex t;
        SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text.c_str(), {255, 255, 255, 255});
        if (surf) {
            t.tex = SDL_CreateTextureFromSurface(ren_, surf);
            t.w = surf->w;
            t.h = surf->h;
            SDL_FreeSurface(surf);
        }
        it = textCache_.emplace(std::move(key), t).first;
    }
    it->second.lastUsed = frame_;
    return it->second.tex ? &it->second : nullptr;
}

void GameRenderer::drawText(TTF_Font* font, const std::string& text, int x, int y, SDL_Color color, bool center, bool shadow) {
    const TextTex* t = getText(font, text);
    if (!t) return;
    int bx = center ? (x - t->w / 2) : x;
    if (shadow) {
        SDL_SetTextureColorMod(t->tex, 18, 18, 18);
        SDL_Rect s = {bx + 3, y + 3, t->w, t->h};
        SDL_RenderCopy(ren_, t->tex, nullptr, &s);
    }
    SDL_SetTextureColorMod(t->tex, color.r, color.g, color.b);
    SDL_Rect d = {bx, y, t->w, t->h};
    SDL_RenderCopy(ren_, t->tex, nullptr, &d);
}

void GameRenderer::drawTextScaled(TTF_Font* font, const std::string& text, float cx, float cy, SDL_Color color,
                                  float scale, float angleDeg, int outline) {
    const TextTex* t = getText(font, text);
    if (!t || scale <= 0.01f) return;
    float w = t->w * scale, h = t->h * scale;
    auto blit = [&](float ox, float oy, SDL_Color c) {
        SDL_SetTextureColorMod(t->tex, c.r, c.g, c.b);
        SDL_FRect d = {cx - w * 0.5f + ox, cy - h * 0.5f + oy, w, h};
        SDL_RenderCopyExF(ren_, t->tex, nullptr, &d, angleDeg, nullptr, SDL_FLIP_NONE);
    };
    if (outline > 0) {
        float o = static_cast<float>(outline);
        blit(o + 4.0f, o + 4.0f, CyberTheme::cBlack); // sombra dura
        const float dirs[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
        for (const auto& d : dirs) blit(d[0] * o, d[1] * o, CyberTheme::cBlack);
    }
    blit(0.0f, 0.0f, color);
}

void GameRenderer::drawStamp(TTF_Font* font, const std::string& text, float cx, float cy, float scale,
                             float angleDeg, SDL_Color fill, SDL_Color textCol) {
    const TextTex* t = getText(font, text);
    if (!t || scale <= 0.01f) return;
    float w = (t->w + 34.0f) * scale;
    float h = (t->h + 10.0f) * scale;
    drawStampBox(cx, cy, w, h, angleDeg, fill, 4, 6);
    drawTextScaled(font, text, cx, cy, textCol, scale, angleDeg, 0);
}

// ===========================================================================
// Fondo dinámico
// ===========================================================================
void GameRenderer::renderBackground(float time, int accentLevel, bool danger) {
    SDL_SetRenderDrawColor(ren_, CyberTheme::cDarkBg.r, CyberTheme::cDarkBg.g, CyberTheme::cDarkBg.b, 255);
    SDL_RenderClear(ren_);

    // 1. Tipografía gigante de fondo desplazándose (marca visual del juego)
    SDL_Color bgText = {26, 30, 46, 255};
    if (accentLevel == 1) bgText = {44, 40, 22, 255};
    else if (accentLevel >= 2) bgText = {54, 24, 42, 255};
    if (danger && blink(time, 4.0f)) bgText = {58, 22, 26, 255};

    const TextTex* big = getText(fontGiant_, "NINJA SLASHER ");
    if (big) {
        SDL_SetTextureColorMod(big->tex, bgText.r, bgText.g, bgText.b);
        float off1 = std::fmod(time * 70.0f, static_cast<float>(big->w));
        for (float x = -off1; x < kW; x += big->w) {
            SDL_Rect d = {static_cast<int>(x), 120, big->w, big->h};
            SDL_RenderCopy(ren_, big->tex, nullptr, &d);
        }
        float off2 = std::fmod(time * 50.0f, static_cast<float>(big->w));
        for (float x = off2 - big->w; x < kW; x += big->w) {
            SDL_Rect d = {static_cast<int>(x), 600, big->w, big->h};
            SDL_RenderCopy(ren_, big->tex, nullptr, &d);
        }
    }

    // 2. Cuadrícula cibernética que sube (sensación de movimiento constante)
    SDL_SetRenderDrawColor(ren_, CyberTheme::cGridLine.r, CyberTheme::cGridLine.g, CyberTheme::cGridLine.b, 255);
    const int step = 80;
    for (int x = 0; x < kW; x += step) {
        SDL_Rect r = {x, 0, 2, kH};
        SDL_RenderFillRect(ren_, &r);
    }
    float gOff = std::fmod(time * 35.0f, static_cast<float>(step));
    for (float y = kH - gOff; y > -step; y -= step) {
        SDL_Rect r = {0, static_cast<int>(y), kW, 2};
        SDL_RenderFillRect(ren_, &r);
    }

    // 3. Figuras geométricas flotantes (parallax)
    SDL_Color deco = {40, 46, 70, 255};
    for (int i = 0; i < 10; ++i) {
        float size = 26.0f + (i % 3) * 18.0f;
        float px = std::fmod(i * 211.0f + 90.0f, static_cast<float>(kW)) + std::sin(time * 0.4f + i) * 40.0f;
        float py = kH + 120.0f - std::fmod(time * (22.0f + i * 5.0f) + i * 137.0f, kH + 240.0f);
        float ang = time * (0.3f + i * 0.07f) + i;
        int corners = (i % 2 == 0) ? 4 : 3;
        float pts[4][2];
        for (int c = 0; c < corners; ++c) {
            float a = ang + c * (2.0f * kPi / corners);
            pts[c][0] = px + std::cos(a) * size;
            pts[c][1] = py + std::sin(a) * size;
        }
        for (int c = 0; c < corners; ++c) {
            int n = (c + 1) % corners;
            renderThickLine(pts[c][0], pts[c][1], pts[n][0], pts[n][1], 5.0f, deco);
        }
    }

    // 4. Bandas de peligro superior e inferior (rojas y parpadeantes en zona crítica)
    SDL_Color bandA = CyberTheme::cYellow;
    SDL_Color bandB = CyberTheme::cBlack;
    if (danger) bandA = blink(time, 5.0f) ? CyberTheme::cRed : CyberTheme::cYellow;
    drawHazardBand(0, 0, kW, 22, time * 90.0f, bandA, bandB);
    drawHazardBand(0, kH - 22, kW, 22, -time * 90.0f, bandA, bandB);
}

// ===========================================================================
// Objetos del juego
// ===========================================================================
void GameRenderer::drawEntity(EntityType type, float cx, float cy, float r, float angle, float idealAngle, float time) {
    const int icx = static_cast<int>(cx), icy = static_cast<int>(cy);
    const SDL_Color& K = CyberTheme::cBlack;

    switch (type) {
    case EntityType::ENERGY_CORE: {
        drawNeobrutalCircle(icx, icy, static_cast<int>(r), CyberTheme::cP1Cyan, 4, 7);
        drawRing(cx, cy, r * 0.66f, r * 0.16f, CyberTheme::cWhite);
        drawRing(cx, cy, r * 0.66f + r * 0.08f + 1.5f, 3.0f, K);
        float pr = r * (0.30f + 0.05f * std::sin(time * 9.0f));
        drawNeobrutalCircle(icx, icy, static_cast<int>(pr), CyberTheme::cYellow, 3, 0);
        for (int i = 0; i < 3; ++i) {
            float a = angle + i * (2.0f * kPi / 3.0f);
            drawNeobrutalCircle(static_cast<int>(cx + std::cos(a) * r * 0.86f),
                                static_cast<int>(cy + std::sin(a) * r * 0.86f), 5, CyberTheme::cWhite, 2, 0);
        }
        drawQuad(cx - r * 0.42f, cy - r * 0.52f, r * 0.30f, r * 0.10f, -35.0f, CyberTheme::cWhite);
        break;
    }
    case EntityType::CYBER_DRONE: {
        float lineAng = idealAngle + angle;
        float armAng = lineAng + kPi * 0.5f;
        for (int side = -1; side <= 1; side += 2) {
            float ex = cx + std::cos(armAng) * side * r * 1.30f;
            float ey = cy + std::sin(armAng) * side * r * 1.30f;
            renderThickLine(cx, cy, ex, ey, 14.0f, K);
            renderThickLine(cx, cy, ex, ey, 6.0f, CyberTheme::cSilver);
            drawNeobrutalCircle(static_cast<int>(ex), static_cast<int>(ey), static_cast<int>(r * 0.26f), CyberTheme::cSilver, 3, 4);
            float ba = time * 28.0f + side;
            renderThickLine(ex - std::cos(ba) * r * 0.42f, ey - std::sin(ba) * r * 0.42f,
                            ex + std::cos(ba) * r * 0.42f, ey + std::sin(ba) * r * 0.42f, 5.0f, K);
        }
        drawNeobrutalCircle(icx, icy, static_cast<int>(r), CyberTheme::cPurple, 4, 7);
        drawNeobrutalCircle(icx, icy, static_cast<int>(r * 0.62f), {76, 29, 149, 255}, 3, 0);
        drawNeobrutalCircle(icx, icy, static_cast<int>(r * 0.24f), CyberTheme::cRed, 3, 0);
        if (!blink(time + cx * 0.001f, 1.2f) || std::fmod(time, 1.6f) > 0.12f) {
            drawNeobrutalCircle(icx - 3, icy - 3, static_cast<int>(r * 0.08f) + 1, CyberTheme::cWhite, 0, 0);
        }
        // Línea vulnerable: cortar en esta dirección da PERFECTO
        float lx = std::cos(lineAng) * r * 1.18f, ly = std::sin(lineAng) * r * 1.18f;
        renderThickLine(cx - lx, cy - ly, cx + lx, cy + ly, 15.0f, K);
        renderThickLine(cx - lx, cy - ly, cx + lx, cy + ly, 7.0f, CyberTheme::cYellow);
        int pulse = 7 + static_cast<int>(2.0f * std::sin(time * 10.0f));
        drawNeobrutalCircle(static_cast<int>(cx + lx), static_cast<int>(cy + ly), pulse, CyberTheme::cYellow, 3, 0);
        drawNeobrutalCircle(static_cast<int>(cx - lx), static_cast<int>(cy - ly), pulse, CyberTheme::cYellow, 3, 0);
        break;
    }
    case EntityType::MECHA_SHURIKEN: {
        auto blades = [&](float ox, float oy, float grow, SDL_Color c) {
            for (int i = 0; i < 4; ++i) {
                float a = angle + i * (kPi * 0.5f);
                float tipR = r * 1.42f * grow, baseR = r * 0.48f * grow;
                fillTriangle(cx + ox + std::cos(a) * tipR, cy + oy + std::sin(a) * tipR,
                             cx + ox + std::cos(a - 0.62f) * baseR, cy + oy + std::sin(a - 0.62f) * baseR,
                             cx + ox + std::cos(a + 0.62f) * baseR, cy + oy + std::sin(a + 0.62f) * baseR, c);
            }
        };
        blades(7.0f, 7.0f, 1.12f, K);
        blades(0.0f, 0.0f, 1.12f, K);
        blades(0.0f, 0.0f, 1.0f, CyberTheme::cYellow);
        drawNeobrutalCircle(icx, icy, static_cast<int>(r * 0.40f), CyberTheme::cWhite, 4, 0);
        drawNeobrutalCircle(icx, icy, static_cast<int>(r * 0.14f), K, 0, 0);
        if (std::fmod(time * 1.7f + cx * 0.003f, 1.0f) < 0.16f) {
            float sx = cx + r * 0.9f, sy = cy - r * 0.9f;
            renderThickLine(sx - 16, sy, sx + 16, sy, 5.0f, CyberTheme::cWhite);
            renderThickLine(sx, sy - 16, sx, sy + 16, 5.0f, CyberTheme::cWhite);
        }
        break;
    }
    case EntityType::EMP_MINE: {
        drawRing(cx, cy, r * 1.62f + 4.0f * std::sin(time * 8.0f), 4.0f, CyberTheme::cRed);
        const int spikes = 8;
        for (int i = 0; i < spikes; ++i) {
            float a = angle + (static_cast<float>(i) / spikes) * 2.0f * kPi;
            float sx = cx + std::cos(a) * r * 1.34f, sy = cy + std::sin(a) * r * 1.34f;
            renderThickLine(cx, cy, sx, sy, 12.0f, K);
            renderThickLine(cx, cy, sx, sy, 5.0f, CyberTheme::cRed);
            drawNeobrutalCircle(static_cast<int>(sx), static_cast<int>(sy), 6, CyberTheme::cYellow, 3, 0);
        }
        drawNeobrutalCircle(icx, icy, static_cast<int>(r), CyberTheme::cRed, 4, 7);
        drawNeobrutalCircle(icx, icy, static_cast<int>(r * 0.56f), K, 0, 0);
        SDL_Color core = blink(time, 6.0f) ? CyberTheme::cYellow : CyberTheme::cWhite;
        drawNeobrutalCircle(icx, icy, static_cast<int>(r * 0.38f), core, 0, 0);
        drawTextScaled(fontHud_, "!", cx, cy, K, 1.0f, 0.0f, 0);
        break;
    }
    case EntityType::POWERUP_FREEZE: {
        // Cápsula hexagonal Criogénica con copo/diamante central
        drawStampBox(cx, cy, r * 1.8f, r * 1.8f, angle * 18.0f, CyberTheme::cIceCyan, 4, 7);
        drawStampBox(cx, cy, r * 1.1f, r * 1.1f, -angle * 28.0f, CyberTheme::cWhite, 3, 0);
        // Cruz de hielo
        renderThickLine(cx - r * 0.45f, cy, cx + r * 0.45f, cy, 6.0f, K);
        renderThickLine(cx, cy - r * 0.45f, cx, cy + r * 0.45f, 6.0f, K);
        drawRing(cx, cy, r * 0.9f + 3.0f * std::sin(time * 8.0f), 3.0f, CyberTheme::cWhite);
        break;
    }
    case EntityType::POWERUP_DUAL: {
        // Cápsula de Doble Filo / Espadas Gemelas
        drawStampBox(cx, cy, r * 1.85f, r * 1.85f, 45.0f, CyberTheme::cYellow, 4, 7);
        drawNeobrutalCircle(icx, icy, static_cast<int>(r * 0.55f), CyberTheme::cBlack, 0, 0);
        // Dos filos cruzados en X
        float kLen = r * 0.55f;
        renderThickLine(cx - kLen, cy - kLen, cx + kLen, cy + kLen, 7.0f, CyberTheme::cWhite);
        renderThickLine(cx + kLen, cy - kLen, cx - kLen, cy + kLen, 7.0f, CyberTheme::cWhite);
        renderThickLine(cx - kLen, cy - kLen, cx + kLen, cy + kLen, 3.0f, CyberTheme::cYellow);
        renderThickLine(cx + kLen, cy - kLen, cx - kLen, cy + kLen, 3.0f, CyberTheme::cYellow);
        break;
    }
    case EntityType::POWERUP_FRENZY: {
        // Cápsula de Fuego / Frenesí (Naranja brillante con núcleo llameante)
        float pulse = 1.0f + 0.12f * std::sin(time * 12.0f);
        drawStampBox(cx, cy, r * 1.8f * pulse, r * 1.8f * pulse, angle * 35.0f, CyberTheme::cNeonFlame, 4, 7);
        drawStampBox(cx, cy, r * 1.0f, r * 1.0f, -angle * 45.0f, CyberTheme::cYellow, 3, 0);
        drawTextScaled(fontHud_, "2X", cx, cy, K, 1.0f * pulse, 0.0f, 0);
        break;
    }
    }
}

void GameRenderer::renderEntities(const std::vector<CyberEntity>& entities, float time) {
    for (const auto& e : entities) {
        if (!e.isAlive()) continue;
        drawEntity(e.getType(), e.getX(), e.getY(), e.getRadius(), e.getAngle(), e.getIdealAngle(), time);
    }
}

void GameRenderer::renderHalves(const std::vector<CutHalf>& halves) {
    for (const auto& h : halves) {
        SDL_Color fill = CyberTheme::cWhite, inner = CyberTheme::cWhite;
        switch (h.type) {
            case EntityType::ENERGY_CORE:    fill = CyberTheme::cP1Cyan; inner = CyberTheme::cYellow; break;
            case EntityType::CYBER_DRONE:    fill = CyberTheme::cPurple; inner = {76, 29, 149, 255}; break;
            case EntityType::MECHA_SHURIKEN: fill = CyberTheme::cYellow; inner = CyberTheme::cWhite; break;
            case EntityType::EMP_MINE:       fill = CyberTheme::cRed;    inner = CyberTheme::cBlack; break;
            case EntityType::POWERUP_FREEZE: fill = CyberTheme::cIceCyan; inner = CyberTheme::cWhite; break;
            case EntityType::POWERUP_DUAL:   fill = CyberTheme::cYellow; inner = CyberTheme::cOrange; break;
            case EntityType::POWERUP_FRENZY: fill = CyberTheme::cNeonFlame; inner = CyberTheme::cYellow; break;
        }
        float startA = h.cutAngle + (h.side > 0 ? 0.0f : kPi);
        float r = h.radius;
        drawHalfDisc(h.x + 7.0f, h.y + 7.0f, r + 4.0f, startA, CyberTheme::cBlack); // sombra dura
        drawHalfDisc(h.x, h.y, r + 4.0f, startA, CyberTheme::cBlack);              // borde
        drawHalfDisc(h.x, h.y, r, startA, fill);
        drawHalfDisc(h.x, h.y, r * 0.55f, startA, inner);

        // Cara del corte: borde negro grueso + filo blanco brillante
        float p1x = h.x + std::cos(startA) * (r + 4.0f), p1y = h.y + std::sin(startA) * (r + 4.0f);
        float p2x = h.x - std::cos(startA) * (r + 4.0f), p2y = h.y - std::sin(startA) * (r + 4.0f);
        renderThickLine(p1x, p1y, p2x, p2y, 8.0f, CyberTheme::cBlack);
        float nx = std::cos(startA + kPi * 0.5f) * 6.0f, ny = std::sin(startA + kPi * 0.5f) * 6.0f;
        renderThickLine(p1x * 0.85f + h.x * 0.15f + nx, p1y * 0.85f + h.y * 0.15f + ny,
                        p2x * 0.85f + h.x * 0.15f + nx, p2y * 0.85f + h.y * 0.15f + ny, 3.0f, CyberTheme::cWhite);
    }
}

void GameRenderer::renderParticles(const std::vector<SparkParticle>& particles) {
    for (const auto& p : particles) {
        float s = p.size * std::clamp(p.life * 2.2f, 0.0f, 1.0f);
        if (s < 1.5f) continue;
        drawStampBox(p.x, p.y, s, s, p.rot, p.color, 2, 3);
    }
}

void GameRenderer::renderRings(const std::vector<ShockRing>& rings) {
    for (const auto& r : rings) {
        float t = r.thickness * r.life;
        if (t < 1.0f) continue;
        drawRing(r.x, r.y, r.radius, t + 7.0f, CyberTheme::cBlack);
        drawRing(r.x, r.y, r.radius, t, r.color);
    }
}

void GameRenderer::renderSlashFlashes(const std::vector<SlashFlash>& flashes) {
    for (const auto& f : flashes) {
        float w = 26.0f * f.life;
        if (w < 1.0f) continue;
        float len = f.length * (0.75f + 0.25f * (1.0f - f.life));
        float dx = std::cos(f.angle) * len * 0.5f, dy = std::sin(f.angle) * len * 0.5f;
        renderThickLine(f.x - dx, f.y - dy, f.x + dx, f.y + dy, w + 10.0f, CyberTheme::cBlack);
        renderThickLine(f.x - dx, f.y - dy, f.x + dx, f.y + dy, w, f.color);
        renderThickLine(f.x - dx * 0.9f, f.y - dy * 0.9f, f.x + dx * 0.9f, f.y + dy * 0.9f, w * 0.38f, CyberTheme::cWhite);
    }
}

void GameRenderer::renderTrails(const std::vector<SlashTrailPoint>& trail, SDL_Color col,
                                bool fingerDown, float fx, float fy, float time,
                                bool isFrenzy, bool isDual) {
    SDL_Color mainCol = isFrenzy ? CyberTheme::cNeonFlame : col;
    SDL_Color innerCol = isFrenzy ? CyberTheme::cYellow : CyberTheme::cWhite;

    for (size_t i = 1; i < trail.size(); ++i) {
        const auto& a = trail[i - 1];
        const auto& b = trail[i];
        float w = (a.width + b.width) * 0.5f;
        if (w < 1.0f) continue;
        renderThickLine(a.x, a.y, b.x, b.y, w + 10.0f, CyberTheme::cBlack);
    }
    for (size_t i = 1; i < trail.size(); ++i) {
        const auto& a = trail[i - 1];
        const auto& b = trail[i];
        float w = (a.width + b.width) * 0.5f;
        if (w < 1.0f) continue;
        renderThickLine(a.x, a.y, b.x, b.y, w, mainCol);
        if (w > 6.0f) renderThickLine(a.x, a.y, b.x, b.y, w * 0.36f, innerCol);
    }
    if (fingerDown) {
        // Ángulo de dirección del movimiento o reposo a 45 grados
        float moveAngleDeg = 45.0f;
        if (trail.size() >= 2) {
            const auto& pPrev = trail[trail.size() - 2];
            const auto& pCur  = trail.back();
            float dx = pCur.x - pPrev.x;
            float dy = pCur.y - pPrev.y;
            if (dx * dx + dy * dy > 4.0f) {
                moveAngleDeg = std::atan2(dy, dx) * (180.0f / kPi);
            }
        }

        float baseScale = isDual ? 1.38f : 1.0f;
        float pulse = 1.0f + 0.08f * std::sin(time * 10.0f);
        float rRing = 24.0f * baseScale * pulse;

        // 1. Anillo táctico exterior limpio con reborde negro duro
        drawRing(fx + 4.0f, fy + 4.0f, rRing, 7.0f, CyberTheme::cBlack); // Sombra dura desplazada
        drawRing(fx, fy, rRing, 8.0f, CyberTheme::cBlack);               // Reborde negro
        drawRing(fx, fy, rRing, 3.5f, mainCol);                          // Relleno neón

        // 2. Cuatro brackets / miras de precisión en cruz que respiran suavemente
        float bracketIn  = rRing - 6.0f;
        float bracketOut = rRing + 9.0f;
        for (int i = 0; i < 4; ++i) {
            float a = i * (kPi * 0.5f) + (kPi * 0.25f); // En las 4 diagonales (45°, 135°, 225°, 315°)
            float x1 = fx + std::cos(a) * bracketIn;
            float y1 = fy + std::sin(a) * bracketIn;
            float x2 = fx + std::cos(a) * bracketOut;
            float y2 = fy + std::sin(a) * bracketOut;
            renderThickLine(x1 + 3.0f, y1 + 3.0f, x2 + 3.0f, y2 + 3.0f, 7.0f, CyberTheme::cBlack);
            renderThickLine(x1, y1, x2, y2, 7.0f, CyberTheme::cBlack);
            renderThickLine(x1, y1, x2, y2, 3.5f, innerCol);
        }

        // 3. Núcleo Kunai / Diamante afilado orientado hacia el movimiento del tajo
        float coreW = 22.0f * baseScale;
        float coreH = 11.0f * baseScale;
        drawStampBox(fx, fy, coreW, coreH, moveAngleDeg, mainCol, 3, 4);
        drawQuad(fx, fy, coreW * 0.52f, coreH * 0.48f, moveAngleDeg, innerCol);
    }
}

void GameRenderer::renderFloatingScores(const std::vector<FloatingScore>& scores) {
    for (const auto& fs : scores) {
        float age = 1.0f - fs.life;
        float scale = (age < 0.12f) ? (1.55f - (age / 0.12f) * 0.55f) : 1.0f;
        if (fs.life < 0.25f) scale *= std::max(0.0f, fs.life / 0.25f);
        float ang = ((static_cast<int>(fs.x) / 7) % 2 == 0) ? -5.0f : 5.0f;
        SDL_Color fill = {static_cast<Uint8>(fs.r), static_cast<Uint8>(fs.g), static_cast<Uint8>(fs.b), 255};
        drawStamp(fontHud_, fs.text, fs.x, fs.y, scale * 0.85f, ang, fill, CyberTheme::cBlack);
    }
}

// ===========================================================================
// HUD
// ===========================================================================
void GameRenderer::renderHudSolo(const HudSoloData& d) {
    const SDL_Color& K = CyberTheme::cBlack;

    // Puntaje con "pop" al sumar
    drawNeobrutalRect(44, 40, 380, 104, CyberTheme::cP1Cyan, 5, 9);
    drawText(fontSmall_, "PUNTAJE", 66, 50, K, false, false);
    drawTextScaled(fontTitle_, std::to_string(d.displayScore), 234, 108, K, 1.0f + d.scorePop * 0.35f, 0.0f, 0);

    // Blindaje: escudos en rombo
    float shakeX = std::sin(d.time * 90.0f) * d.lifeShake * 12.0f;
    drawNeobrutalRect(456, 40, 270, 104, CyberTheme::cWhite, 5, 9);
    drawText(fontSmall_, "BLINDAJE", 478, 50, K, false, false);
    for (int l = 0; l < 3; ++l) {
        float cx = 512.0f + l * 76.0f + shakeX;
        bool alive = l < d.lives;
        SDL_Color c = alive ? CyberTheme::cGreen : CyberTheme::cSilver;
        if (alive && d.lives == 1 && blink(d.time, 4.0f)) c = CyberTheme::cRed;
        drawStampBox(cx, 108.0f, 36.0f, 36.0f, 45.0f, c, 4, 4);
        if (!alive) {
            renderThickLine(cx - 12, 96, cx + 12, 120, 5.0f, K);
            renderThickLine(cx + 12, 96, cx - 12, 120, 5.0f, K);
        }
    }

    // Récord
    drawNeobrutalRect(kW - 364, 40, 320, 104, CyberTheme::cYellow, 5, 9);
    drawText(fontSmall_, "RÉCORD", kW - 342, 50, K, false, false);
    drawTextScaled(fontTitle_, std::to_string(std::max(d.best, d.displayScore)), kW - 204.0f, 108.0f, K, 1.0f, 0.0f, 0);

    // Combo: sello grande que late + barra de tiempo restante
    if (d.combo > 1) {
        SDL_Color tc = comboTierColor(d.combo);
        float pop = 1.0f + std::max(0.0f, d.comboFrac - 0.82f) * 3.2f;
        float wob = std::sin(d.time * 7.0f) * 2.0f;
        float cx = kW - 220.0f, cy = 290.0f;
        drawStampBox(cx, cy, 300.0f * pop, 150.0f * pop, -7.0f + wob, tc, 5, 10);
        drawTextScaled(fontHud_, comboTierLabel(d.combo), cx, cy - 46.0f * pop, K, pop, -7.0f + wob, 0);
        drawTextScaled(fontTitleBig_, "x" + std::to_string(d.combo), cx, cy + 14.0f * pop, K, 0.85f * pop, -7.0f + wob, 0);
        drawNeobrutalRect(kW - 360, 392, 280, 22, CyberTheme::cWhite, 4, 5);
        drawNeobrutalRect(kW - 360, 392, static_cast<int>(280 * clamp01(d.comboFrac)), 22, tc, 0, 0);
    }

    // Indicadores activos de Power-ups (Sellos flotantes de estado)
    float badgeY = 175.0f;
    if (d.freezeTimer > 0.0f) {
        float fP = 1.0f + 0.08f * std::sin(d.time * 12.0f);
        drawStamp(fontSmall_, "❄ CONGELAR: " + std::to_string(static_cast<int>(std::ceil(d.freezeTimer))) + "s",
                  180.0f, badgeY, fP, -3.0f, CyberTheme::cIceCyan, K);
        badgeY += 46.0f;
    }
    if (d.dualTimer > 0.0f) {
        float dP = 1.0f + 0.08f * std::sin(d.time * 12.0f);
        drawStamp(fontSmall_, "⚔ FILO DOBLE: " + std::to_string(static_cast<int>(std::ceil(d.dualTimer))) + "s",
                  180.0f, badgeY, dP, 3.0f, CyberTheme::cYellow, K);
        badgeY += 46.0f;
    }
    if (d.frenzyTimer > 0.0f) {
        float rP = 1.0f + 0.08f * std::sin(d.time * 12.0f);
        drawStamp(fontSmall_, "🔥 FRENESÍ X2: " + std::to_string(static_cast<int>(std::ceil(d.frenzyTimer))) + "s",
                  180.0f, badgeY, rP, -2.0f, CyberTheme::cNeonFlame, K);
        badgeY += 46.0f;
    }
}

void GameRenderer::drawTugBar(int x, int y, int w, int h, int s1, int s2) {
    int total = s1 + s2;
    float frac = (total > 0) ? static_cast<float>(s1) / total : 0.5f;
    int split = static_cast<int>(w * frac);
    drawNeobrutalRect(x, y, w, h, CyberTheme::cP2Pink, 4, 6);
    drawNeobrutalRect(x, y, split, h, CyberTheme::cP1Cyan, 0, 0);
    SDL_SetRenderDrawColor(ren_, 18, 18, 18, 255);
    SDL_Rect mid = {x + w / 2 - 2, y, 4, h};
    SDL_RenderFillRect(ren_, &mid);
    drawStampBox(static_cast<float>(x + split), y + h * 0.5f, 26.0f, 26.0f, 45.0f, CyberTheme::cWhite, 4, 4);
}

void GameRenderer::renderHud1v1(const Hud1v1Data& d) {
    const SDL_Color& K = CyberTheme::cBlack;
    const SDL_Color boxCol[2] = {CyberTheme::cP1Cyan, CyberTheme::cP2Pink};
    const char* names[2] = {"JUGADOR 1", "JUGADOR 2"};
    const int bx[2] = {44, kW - 464};

    for (int p = 0; p < 2; ++p) {
        drawNeobrutalRect(bx[p], 40, 420, 110, boxCol[p], 5, 9);
        drawText(fontSmall_, names[p], bx[p] + 22, 50, K, false, false);
        drawTextScaled(fontTitle_, std::to_string(d.displayScore[p]), bx[p] + 210.0f, 112.0f, K,
                       1.0f + d.scorePop[p] * 0.35f, 0.0f, 0);
        if (d.combo[p] > 1) {
            float pop = 1.0f + std::max(0.0f, d.comboFrac[p] - 0.82f) * 3.0f;
            std::string txt = "x" + std::to_string(d.combo[p]) + " " + comboTierLabel(d.combo[p]);
            drawStamp(fontHud_, txt, bx[p] + 210.0f, 210.0f, pop, (p == 0) ? -5.0f : 5.0f,
                      comboTierColor(d.combo[p]), K);
        }

        // Badges de powerup activo para cada jugador
        float py = 265.0f;
        if (d.dualTimer[p] > 0.0f) {
            drawStamp(fontSmall_, "⚔ FILO DOBLE", bx[p] + 210.0f, py, 1.0f, 0.0f, CyberTheme::cYellow, K);
            py += 38.0f;
        }
        if (d.frenzyTimer[p] > 0.0f) {
            drawStamp(fontSmall_, "🔥 FRENESÍ X2", bx[p] + 210.0f, py, 1.0f, 0.0f, CyberTheme::cNeonFlame, K);
            py += 38.0f;
        }
    }

    // Temporizador central (late en rojo los últimos 10 segundos)
    bool hurry = d.timeLeft <= 10.0f;
    SDL_Color tCol = (hurry && blink(d.time, 4.0f)) ? CyberTheme::cRed : CyberTheme::cYellow;
    float tPop = hurry ? 1.0f + 0.12f * std::max(0.0f, std::sin(d.time * 12.0f)) : 1.0f;
    drawStampBox(960.0f, 96.0f, 210.0f * tPop, 112.0f * tPop, 0.0f, tCol, 5, 9);
    drawTextScaled(fontSmall_, "TIEMPO", 960.0f, 60.0f, K, 1.0f, 0.0f, 0);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d", std::max(0, static_cast<int>(std::ceil(d.timeLeft))));
    drawTextScaled(fontTitleBig_, buf, 960.0f, 108.0f, K, 0.62f * tPop, 0.0f, 0);

    // Barra de dominio (tira y afloja de puntos)
    drawTugBar(560, 172, 800, 26, d.score[0], d.score[1]);

    // Estado global de congelación si está activo en 1v1
    if (d.freezeTimer > 0.0f) {
        float fP = 1.0f + 0.08f * std::sin(d.time * 12.0f);
        drawStamp(fontSmall_, "❄ TIEMPO RALENTIZADO ❄", 960.0f, 215.0f, fP, 0.0f, CyberTheme::cIceCyan, K);
    }
}

// ===========================================================================
// Menú principal (con objetos volando de fondo en modo "attract")
// ===========================================================================
void GameRenderer::drawTitleBounce(const std::string& text, float cx, float cy, float time, float anim) {
    std::vector<const TextTex*> glyphs;
    float total = 0.0f;
    for (char ch : text) {
        std::string s(1, ch);
        const TextTex* t = (ch == ' ') ? nullptr : getText(fontTitleBig_, s);
        glyphs.push_back(t);
        total += (t ? t->w : 40.0f) + 6.0f;
    }
    float x = cx - total * 0.5f;
    int nonSpace = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        const TextTex* t = glyphs[i];
        float w = (t ? t->w : 40.0f);
        if (t) {
            float local = clamp01((anim * 1.8f - i * 0.06f) / 0.45f);
            float drop = (1.0f - easeOutBack(local)) * -320.0f;
            float bob = std::sin(time * 4.5f + i * 0.55f) * 10.0f;
            float tilt = std::sin(time * 3.0f + i) * 4.0f;
            SDL_Color c = (nonSpace < 5) ? CyberTheme::cP1Cyan : CyberTheme::cP2Pink;
            if (local > 0.0f) {
                drawTextScaled(fontTitleBig_, std::string(1, text[i]), x + w * 0.5f, cy + drop + bob, c, 1.0f, tilt, 5);
            }
            ++nonSpace;
        }
        x += w + 6.0f;
    }
}

void GameRenderer::drawPadlock(int cx, int cy) {
    drawRing(static_cast<float>(cx), cy - 14.0f, 16.0f, 7.0f, CyberTheme::cBlack);
    drawNeobrutalRect(cx - 22, cy - 10, 44, 34, CyberTheme::cBlack, 0, 4);
    drawNeobrutalRect(cx - 4, cy, 8, 12, CyberTheme::cSilver, 0, 0);
}

void GameRenderer::renderMenu(const MenuData& d) {
    const SDL_Color& K = CyberTheme::cBlack;

    // Título con letras que caen y rebotan
    drawTitleBounce("NINJA SLASHER", 960.0f, 150.0f, d.time, d.anim);

    // Cinta subtitulada
    float rib = easeOutBack(clamp01(d.anim * 2.0f - 0.6f));
    if (rib > 0.0f) {
        drawStampBox(960.0f, 268.0f, 660.0f * rib, 58.0f, -3.0f, CyberTheme::cYellow, 5, 8);
        drawTextScaled(fontHud_, "CYBER KATANA EDITION", 960.0f, 268.0f, K, rib, -3.0f, 0);
    }

    // Paneles de modo (entran deslizando desde los costados)
    float slide = easeOutBack(clamp01(d.anim * 1.6f - 0.35f));
    for (int m = 0; m < 2; ++m) {
        bool sel = (d.selected == m);
        bool locked = (m == 1 && !d.versusUnlocked);
        float dir = (m == 0) ? -1.0f : 1.0f;
        int pw = 540, ph = 250;
        int px = static_cast<int>(960 + dir * 300 - pw / 2 + (1.0f - slide) * dir * 1300.0f);
        int py = 350 - (sel ? 10 : 0);
        SDL_Color fill = locked ? CyberTheme::cSilver : ((m == 0) ? CyberTheme::cP1Cyan : CyberTheme::cP2Pink);

        drawNeobrutalRect(px, py, pw, ph, fill, sel ? 7 : 4, sel ? 16 : 7);
        int cx = px + pw / 2;
        if (m == 0) {
            drawTextScaled(fontTitle_, "MODO SOLO", static_cast<float>(cx), py + 52.0f, K, 1.0f, 0.0f, 0);
            drawText(fontSmall_, "CORTÁ TODO LO QUE PUEDAS", cx, py + 98, K, true, false);
            drawText(fontSmall_, "3 VIDAS  ·  SIN LÍMITE DE TIEMPO", cx, py + 130, K, true, false);
        } else {
            drawTextScaled(fontTitle_, "1 VS 1", static_cast<float>(cx), py + 52.0f, K, 1.0f, 0.0f, 0);
            if (locked) {
                drawPadlock(cx, py + 122);
                drawText(fontSmall_, "CONECTÁ UN 2º DUALSHOCK", cx, py + 158, K, true, false);
            } else {
                drawText(fontSmall_, "DUELO CON DOS DUALSHOCK", cx, py + 98, K, true, false);
                drawText(fontSmall_, "60 SEGUNDOS  ·  MÁS PUNTOS GANA", cx, py + 130, K, true, false);
            }
        }
        if (sel && !locked) {
            float bounce = std::abs(std::sin(d.time * 6.0f)) * 6.0f;
            drawStamp(fontHud_, "[X] JUGAR", static_cast<float>(cx), py + ph - 40.0f - bounce, 1.0f, 0.0f, CyberTheme::cWhite, K);
        }
    }

    // Instrucciones de navegación
    if (d.anim > 0.6f) {
        std::string hint = d.versusUnlocked ? "D-PAD: ELEGIR MODO   ·   [X]: JUGAR   ·   TOUCHPAD: PRACTICÁ EL CORTE"
                                            : "[X]: JUGAR   ·   TOUCHPAD: PRACTICÁ EL CORTE CON LOS OBJETOS";
        drawText(fontSmall_, hint, 960, 638, CyberTheme::cWhite, true, true);
    }

    // Tarjetas de tutorial: enseñan los 4 objetos de un vistazo
    struct Card { EntityType type; const char* name; const char* desc; SDL_Color fill; };
    const Card cards[4] = {
        {EntityType::ENERGY_CORE,    "NÚCLEO",   "CORTALO",          CyberTheme::cWhite},
        {EntityType::CYBER_DRONE,    "DRON",     "SEGUÍ LA LÍNEA",   CyberTheme::cWhite},
        {EntityType::MECHA_SHURIKEN, "SHURIKEN", "BONUS DORADO",     CyberTheme::cWhite},
        {EntityType::EMP_MINE,       "MINA PEM", "¡NO LA TOQUES!",   CyberTheme::cRed}};
    for (int i = 0; i < 4; ++i) {
        float appear = easeOutBack(clamp01(d.anim * 1.8f - 0.7f - i * 0.08f));
        if (appear <= 0.0f) continue;
        int cw = 370, ch = 124;
        int cx = static_cast<int>(960 + (i - 1.5f) * 405) - cw / 2;
        int cy = 700 + static_cast<int>((1.0f - appear) * 400.0f);
        drawNeobrutalRect(cx, cy, cw, ch, cards[i].fill, 4, 7);
        float iconAng = d.time * 0.8f;
        float iconR = (cards[i].type == EntityType::CYBER_DRONE) ? 28.0f : 32.0f;
        drawEntity(cards[i].type, cx + 74.0f, cy + ch * 0.5f, iconR, iconAng, 0.6f, d.time);
        drawText(fontHud_, cards[i].name, cx + 146, cy + 26, K, false, false);
        drawText(fontSmall_, cards[i].desc, cx + 146, cy + 72, K, false, false);
    }

    // Estado de mandos + récord
    if (d.anim > 0.8f) {
        std::string p1 = d.p1Connected ? (d.p1IsArcade ? "P1: FIGHT STICK" : "P1: DUALSHOCK 4") : "P1: SIN MANDO";
        std::string p2 = d.p2Connected ? "P2: DUALSHOCK 4" : "P2: DESCONECTADO";
        drawNeobrutalRect(180, 892, 460, 62, d.p1Connected ? CyberTheme::cP1Cyan : CyberTheme::cSilver, 4, 7);
        drawText(fontHud_, p1, 410, 904, K, true, false);
        drawNeobrutalRect(730, 892, 460, 62, CyberTheme::cYellow, 4, 7);
        drawText(fontHud_, "RÉCORD: " + std::to_string(d.best), 960, 904, K, true, false);
        drawNeobrutalRect(1280, 892, 460, 62, d.p2Connected ? CyberTheme::cP2Pink : CyberTheme::cSilver, 4, 7);
        drawText(fontHud_, p2, 1510, 904, K, true, false);
    }
}

// ===========================================================================
// Cuenta regresiva 3-2-1-¡YA!
// ===========================================================================
void GameRenderer::renderCountdown(int number, float beatT) {
    const SDL_Color cols[4] = {CyberTheme::cGreen, CyberTheme::cYellow, CyberTheme::cP2Pink, CyberTheme::cP1Cyan};
    int ci = std::clamp(number, 0, 3);
    float s = easeOutBack(clamp01(beatT / 0.28f));
    if (beatT > 0.8f) s *= std::max(0.0f, 1.0f - (beatT - 0.8f) / 0.2f);
    if (s <= 0.01f) return;
    float ang = (number % 2 == 0) ? 6.0f : -6.0f;
    std::string txt = (number > 0) ? std::to_string(number) : "¡YA!";
    float w = (number > 0) ? 340.0f : 680.0f;
    drawStampBox(960.0f, 520.0f, w * s, 300.0f * s, ang, cols[ci], 7, 16);
    drawTextScaled(fontGiant_, txt, 960.0f, 520.0f, CyberTheme::cBlack, s, ang, 0);
    if (number > 0) {
        drawStamp(fontHud_, "PREPARÁ LA KATANA", 960.0f, 740.0f, 1.0f, -2.0f, CyberTheme::cWhite, CyberTheme::cBlack);
    }
}

// ===========================================================================
// Fin de partida
// ===========================================================================
void GameRenderer::renderGameOver(const GameOverData& d) {
    const SDL_Color& K = CyberTheme::cBlack;
    float drop = easeOutBounce(clamp01(d.anim / 0.75f));
    int top = 190 + static_cast<int>((1.0f - drop) * -1000.0f);
    int pw = 1120, ph = 660;
    int px = 960 - pw / 2;

    drawNeobrutalRect(px, top, pw, ph, CyberTheme::cWhite, 7, 16);

    if (!d.versus) {
        drawStampBox(960.0f, top + 8.0f, 760.0f, 92.0f, -3.0f, CyberTheme::cRed, 5, 9);
        drawTextScaled(fontTitle_, "FIN DE LA PARTIDA", 960.0f, top + 8.0f, K, 1.0f, -3.0f, 0);

        drawText(fontHud_, "PUNTAJE", 780, top + 92, K, true, false);
        drawTextScaled(fontGiant_, std::to_string(d.displayScore[0]), 780.0f, top + 230.0f, K, 0.78f, 0.0f, 0);

        // Rango (S/A/B/C/D) que cae como sello al terminar el conteo
        if (d.rankReveal > 0.0f) {
            float s = 2.2f - 1.2f * easeOutBack(d.rankReveal);
            drawStampBox(1300.0f, top + 230.0f, 230.0f * s, 230.0f * s, 10.0f, rankColor(d.rank), 6, 12);
            drawTextScaled(fontGiant_, std::string(1, d.rank), 1300.0f, top + 236.0f, K, 0.9f * s, 10.0f, 0);
            drawTextScaled(fontSmall_, "RANGO", 1300.0f, top + 108.0f, K, 1.0f, 0.0f, 0);
        }

        const char* labels[3] = {"CORTES", "PERFECTOS", "COMBO MÁX"};
        int vals[3] = {d.cuts[0], d.perfects[0], d.maxCombo[0]};
        SDL_Color fills[3] = {CyberTheme::cP1Cyan, CyberTheme::cGreen, CyberTheme::cYellow};
        for (int i = 0; i < 3; ++i) {
            int bxc = 960 + (i - 1) * 350;
            drawNeobrutalRect(bxc - 150, top + 380, 300, 116, fills[i], 4, 7);
            drawText(fontSmall_, labels[i], bxc, top + 392, K, true, false);
            drawTextScaled(fontTitle_, std::to_string(vals[i]), static_cast<float>(bxc), top + 456.0f, K, 1.0f, 0.0f, 0);
        }

        if (d.newRecord) {
            if (blink(d.time, 3.0f)) {
                drawStamp(fontTitle_, "¡NUEVO RÉCORD!", 960.0f, top + 580.0f, 1.0f, 3.0f, CyberTheme::cYellow, K);
            }
        } else {
            drawText(fontHud_, "RÉCORD: " + std::to_string(d.best), 960, top + 562, K, true, false);
        }
    } else {
        int s1 = d.score[0], s2 = d.score[1];
        int winner = (s1 > s2) ? 0 : ((s2 > s1) ? 1 : -1);
        SDL_Color hc = (winner == 0) ? CyberTheme::cP1Cyan : ((winner == 1) ? CyberTheme::cP2Pink : CyberTheme::cYellow);
        const char* head = (winner == 0) ? "¡GANA JUGADOR 1!" : ((winner == 1) ? "¡GANA JUGADOR 2!" : "¡EMPATE!");
        drawStampBox(960.0f, top + 8.0f, 760.0f, 92.0f, -3.0f, hc, 5, 9);
        drawTextScaled(fontTitle_, head, 960.0f, top + 8.0f, K, 1.0f, -3.0f, 0);

        const SDL_Color cols[2] = {CyberTheme::cP1Cyan, CyberTheme::cP2Pink};
        const char* names[2] = {"JUGADOR 1", "JUGADOR 2"};
        for (int p = 0; p < 2; ++p) {
            int cx = 960 + (p == 0 ? -280 : 280);
            drawNeobrutalRect(cx - 230, top + 100, 460, 300, cols[p], 5, 9);
            drawText(fontHud_, names[p], cx, top + 116, K, true, false);
            drawTextScaled(fontTitleBig_, std::to_string(d.displayScore[p]), static_cast<float>(cx), top + 240.0f, K, 0.9f, 0.0f, 0);
            char stats[96];
            std::snprintf(stats, sizeof(stats), "CORTES %d  ·  PERF %d  ·  COMBO %d", d.cuts[p], d.perfects[p], d.maxCombo[p]);
            drawText(fontSmall_, stats, cx, top + 348, K, true, false);
            if (winner == p && d.rankReveal > 0.0f) {
                float s = 2.0f - easeOutBack(d.rankReveal);
                drawStamp(fontTitle_, "GANADOR", static_cast<float>(cx), top + 96.0f, s, (p == 0) ? -8.0f : 8.0f, CyberTheme::cYellow, K);
            }
        }
        drawStamp(fontTitle_, "VS", 960.0f, top + 250.0f, 1.0f, -6.0f, CyberTheme::cYellow, K);
        drawTugBar(260, top + 460, 1400, 40, std::max(0, d.displayScore[0]), std::max(0, d.displayScore[1]));
    }

    if (d.anim > 1.2f && blink(d.time, 2.0f)) {
        drawStamp(fontHud_, "PRESIONÁ [X] PARA VOLVER A JUGAR", 960.0f, top + ph + 70.0f, 1.0f, 0.0f, CyberTheme::cWhite, K);
    }
}
