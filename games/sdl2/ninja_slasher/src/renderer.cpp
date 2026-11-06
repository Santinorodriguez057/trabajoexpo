#include "renderer.h"
#include <cmath>
#include <algorithm>

namespace {
    constexpr float kPi = 3.1415926535f;
}

GameRenderer::GameRenderer(SDL_Renderer* ren, TTF_Font* fontTitle, TTF_Font* fontHud, TTF_Font* fontSmall)
    : ren_(ren), fontTitle_(fontTitle), fontHud_(fontHud), fontSmall_(fontSmall) {
    whiteTex_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, 1, 1);
    if (whiteTex_) {
        Uint32 pixel = 0xFFFFFFFF;
        SDL_UpdateTexture(whiteTex_, nullptr, &pixel, sizeof(Uint32));
    }
}

GameRenderer::~GameRenderer() {
    for (auto& kv : textCache_) {
        if (kv.second.tex) SDL_DestroyTexture(kv.second.tex);
    }
    textCache_.clear();
    if (whiteTex_) SDL_DestroyTexture(whiteTex_);
}

void GameRenderer::drawNeobrutalRect(int x, int y, int w, int h, SDL_Color fill, int borderW, int shadowOffset) {
    if (w <= 0 || h <= 0) return;

    // 1. Sombra dura desplazada
    if (shadowOffset > 0) {
        SDL_Rect sR = {x + shadowOffset, y + shadowOffset, w, h};
        SDL_SetRenderDrawColor(ren_, CyberTheme::cBlack.r, CyberTheme::cBlack.g, CyberTheme::cBlack.b, 255);
        SDL_RenderFillRect(ren_, &sR);
    }

    // 2. Borde exterior negro grueso
    if (borderW > 0) {
        SDL_Rect bR = {x - borderW, y - borderW, w + borderW * 2, h + borderW * 2};
        SDL_SetRenderDrawColor(ren_, CyberTheme::cBlack.r, CyberTheme::cBlack.g, CyberTheme::cBlack.b, 255);
        SDL_RenderFillRect(ren_, &bR);
    }

    // 3. Relleno plano
    SDL_Rect mR = {x, y, w, h};
    SDL_SetRenderDrawColor(ren_, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(ren_, &mR);
}

void GameRenderer::drawNeobrutalCircle(int cx, int cy, int radius, SDL_Color fill, int borderW, int shadowOffset) {
    if (radius <= 0) return;

    auto fillCircle = [&](int px, int py, int r, SDL_Color col) {
        const int segs = std::clamp(r / 2 + 16, 20, 48);
        std::vector<SDL_Vertex> verts(segs + 1);
        std::vector<int> idx(segs * 3);

        verts[0].position = {static_cast<float>(px), static_cast<float>(py)};
        verts[0].color = col;
        verts[0].tex_coord = {0, 0};

        for (int i = 0; i < segs; ++i) {
            float a = (static_cast<float>(i) / segs) * 2.0f * kPi;
            verts[i + 1].position = {px + std::cos(a) * r, py + std::sin(a) * r};
            verts[i + 1].color = col;
            verts[i + 1].tex_coord = {0, 0};

            idx[i * 3 + 0] = 0;
            idx[i * 3 + 1] = i + 1;
            idx[i * 3 + 2] = (i + 1 == segs) ? 1 : (i + 2);
        }
        SDL_RenderGeometry(ren_, nullptr, verts.data(), static_cast<int>(verts.size()), idx.data(), static_cast<int>(idx.size()));
    };

    if (shadowOffset > 0) {
        fillCircle(cx + shadowOffset, cy + shadowOffset, radius + borderW, CyberTheme::cBlack);
    }
    if (borderW > 0) {
        fillCircle(cx, cy, radius + borderW, CyberTheme::cBlack);
    }
    fillCircle(cx, cy, radius, fill);
}

void GameRenderer::renderThickLine(float x1, float y1, float x2, float y2, float thickness, SDL_Color color) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = std::hypot(dx, dy);
    if (len < 0.001f) return;

    float nx = (-dy / len) * (thickness * 0.5f);
    float ny = ( dx / len) * (thickness * 0.5f);

    SDL_Vertex verts[4];
    verts[0].position = {x1 + nx, y1 + ny}; verts[0].color = color; verts[0].tex_coord = {0, 0};
    verts[1].position = {x2 + nx, y2 + ny}; verts[1].color = color; verts[1].tex_coord = {0, 0};
    verts[2].position = {x2 - nx, y2 - ny}; verts[2].color = color; verts[2].tex_coord = {0, 0};
    verts[3].position = {x1 - nx, y1 - ny}; verts[3].color = color; verts[3].tex_coord = {0, 0};

    int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(ren_, nullptr, verts, 4, idx, 6);
}

void GameRenderer::drawText(TTF_Font* font, const std::string& text, int x, int y, SDL_Color color, bool center) {
    if (!font || text.empty()) return;

    char keyBuf[64];
    std::snprintf(keyBuf, sizeof(keyBuf), "%p_%02X%02X%02X_", (void*)font, color.r, color.g, color.b);
    std::string key = std::string(keyBuf) + text;

    auto it = textCache_.find(key);
    if (it == textCache_.end()) {
        if (textCache_.size() > 400) {
            for (auto& kv : textCache_) if (kv.second.tex) SDL_DestroyTexture(kv.second.tex);
            textCache_.clear();
        }

        TextCacheEntry entry;
        SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text.c_str(), color);
        if (surf) {
            entry.tex = SDL_CreateTextureFromSurface(ren_, surf);
            entry.w = surf->w;
            entry.h = surf->h;
            SDL_FreeSurface(surf);
        }
        it = textCache_.emplace(std::move(key), entry).first;
    }

    const auto& entry = it->second;
    if (!entry.tex) return;

    int bx = center ? (x - entry.w / 2) : x;

    // Sombra negra desplazada
    SDL_Rect sR = {bx + 3, y + 3, entry.w, entry.h};
    SDL_SetTextureColorMod(entry.tex, 18, 18, 18);
    SDL_RenderCopy(ren_, entry.tex, nullptr, &sR);

    // Texto original
    SDL_Rect tR = {bx, y, entry.w, entry.h};
    SDL_SetTextureColorMod(entry.tex, color.r, color.g, color.b);
    SDL_RenderCopy(ren_, entry.tex, nullptr, &tR);
}

void GameRenderer::renderBackground(int w, int h, float time) {
    // Fondo oscuro con cuadrícula cibernética y acentos neobrutalistas
    SDL_SetRenderDrawColor(ren_, CyberTheme::cDarkBg.r, CyberTheme::cDarkBg.g, CyberTheme::cDarkBg.b, 255);
    SDL_RenderClear(ren_);

    // Cuadrícula geométrica
    SDL_SetRenderDrawColor(ren_, CyberTheme::cGridLine.r, CyberTheme::cGridLine.g, CyberTheme::cGridLine.b, 255);
    const int step = 80;
    for (int x = 0; x < w; x += step) {
        SDL_RenderDrawLine(ren_, x, 0, x, h);
    }
    for (int y = 0; y < h; y += step) {
        SDL_RenderDrawLine(ren_, 0, y, w, y);
    }

    // Marco exterior neobrutalista grueso en los bordes de la pantalla
    drawNeobrutalRect(0, 0, w, 12, CyberTheme::cYellow, 0, 0);
    drawNeobrutalRect(0, h - 12, w, 12, CyberTheme::cYellow, 0, 0);
}

void GameRenderer::drawEnergyCore(float cx, float cy, float radius, float angle) {
    // Esfera cibernética cian con núcleo giratorio
    drawNeobrutalCircle(static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(radius), CyberTheme::cP1Cyan, 4, 6);
    drawNeobrutalCircle(static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(radius * 0.65f), CyberTheme::cWhite, 3, 0);
    drawNeobrutalCircle(static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(radius * 0.35f), CyberTheme::cP1Cyan, 2, 0);

    // Acentos radiales
    for (int i = 0; i < 4; ++i) {
        float a = angle + i * (kPi * 0.5f);
        float px = cx + std::cos(a) * (radius * 0.78f);
        float py = cy + std::sin(a) * (radius * 0.78f);
        drawNeobrutalCircle(static_cast<int>(px), static_cast<int>(py), 5, CyberTheme::cBlack, 1, 0);
    }
}

void GameRenderer::drawCyberDrone(float cx, float cy, float radius, float angle, float idealAngle) {
    // Dron blindado púrpura con línea amarilla de corte vulnerable
    drawNeobrutalCircle(static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(radius), CyberTheme::cPurple, 4, 6);
    drawNeobrutalCircle(static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(radius * 0.55f), {76, 29, 149, 255}, 2, 0);

    // Línea de corte vulnerable en ángulo ideal
    float totalAng = idealAngle + angle;
    float lx1 = cx + std::cos(totalAng) * radius;
    float ly1 = cy + std::sin(totalAng) * radius;
    float lx2 = cx - std::cos(totalAng) * radius;
    float ly2 = cy - std::sin(totalAng) * radius;

    renderThickLine(lx1, ly1, lx2, ly2, 8.0f, CyberTheme::cBlack);
    renderThickLine(lx1, ly1, lx2, ly2, 4.0f, CyberTheme::cYellow);

    // Marcas de flechas en los bordes indicando la línea de tajo
    drawNeobrutalCircle(static_cast<int>(lx1), static_cast<int>(ly1), 7, CyberTheme::cYellow, 2, 0);
    drawNeobrutalCircle(static_cast<int>(lx2), static_cast<int>(ly2), 7, CyberTheme::cYellow, 2, 0);
}

void GameRenderer::drawMechaShuriken(float cx, float cy, float radius, float angle) {
    // Shuriken dorado de 4 puntas afiladas
    int r = static_cast<int>(radius);
    drawNeobrutalCircle(static_cast<int>(cx), static_cast<int>(cy), r / 2, CyberTheme::cYellow, 4, 6);

    for (int i = 0; i < 4; ++i) {
        float a1 = angle + i * (kPi * 0.5f);
        float a2 = a1 + (kPi * 0.25f);
        float tipX = cx + std::cos(a1) * (radius * 1.35f);
        float tipY = cy + std::sin(a1) * (radius * 1.35f);
        float midX = cx + std::cos(a2) * (radius * 0.55f);
        float midY = cy + std::sin(a2) * (radius * 0.55f);

        renderThickLine(cx, cy, tipX, tipY, 6.0f, CyberTheme::cBlack);
        renderThickLine(tipX, tipY, midX, midY, 5.0f, CyberTheme::cBlack);
        renderThickLine(cx, cy, tipX, tipY, 3.0f, CyberTheme::cYellow);
    }
    drawNeobrutalCircle(static_cast<int>(cx), static_cast<int>(cy), 8, CyberTheme::cWhite, 2, 0);
}

void GameRenderer::drawEmpMine(float cx, float cy, float radius, float angle) {
    // Mina roja con pinchos de advertencia
    int spikes = 8;
    for (int i = 0; i < spikes; ++i) {
        float a = angle + (static_cast<float>(i) / spikes) * 2.0f * kPi;
        float sx = cx + std::cos(a) * (radius * 1.35f);
        float sy = cy + std::sin(a) * (radius * 1.35f);
        renderThickLine(cx, cy, sx, sy, 7.0f, CyberTheme::cBlack);
        renderThickLine(cx, cy, sx, sy, 3.5f, CyberTheme::cRed);
        drawNeobrutalCircle(static_cast<int>(sx), static_cast<int>(sy), 5, CyberTheme::cYellow, 1, 0);
    }

    drawNeobrutalCircle(static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(radius), CyberTheme::cRed, 4, 6);
    drawNeobrutalCircle(static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(radius * 0.45f), CyberTheme::cBlack, 2, 0);
    drawNeobrutalCircle(static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(radius * 0.20f), CyberTheme::cYellow, 1, 0);
}

void GameRenderer::renderEntities(const std::vector<CyberEntity>& entities) {
    for (const auto& e : entities) {
        if (!e.isAlive()) continue;
        switch (e.getType()) {
            case EntityType::ENERGY_CORE:
                drawEnergyCore(e.getX(), e.getY(), e.getRadius(), e.getAngle());
                break;
            case EntityType::CYBER_DRONE:
                drawCyberDrone(e.getX(), e.getY(), e.getRadius(), e.getAngle(), e.getIdealAngle());
                break;
            case EntityType::MECHA_SHURIKEN:
                drawMechaShuriken(e.getX(), e.getY(), e.getRadius(), e.getAngle());
                break;
            case EntityType::EMP_MINE:
                drawEmpMine(e.getX(), e.getY(), e.getRadius(), e.getAngle());
                break;
        }
    }
}

void GameRenderer::renderHalves(const std::vector<CutHalf>& halves) {
    for (const auto& h : halves) {
        SDL_Color fill = CyberTheme::cWhite;
        if (h.type == EntityType::ENERGY_CORE) fill = CyberTheme::cP1Cyan;
        else if (h.type == EntityType::CYBER_DRONE) fill = CyberTheme::cPurple;
        else if (h.type == EntityType::MECHA_SHURIKEN) fill = CyberTheme::cYellow;
        else if (h.type == EntityType::EMP_MINE) fill = CyberTheme::cRed;

        // Renderizado del semicírculo cortado
        const int segs = 16;
        std::vector<SDL_Vertex> verts(segs + 1);
        std::vector<int> idx((segs - 1) * 3);

        verts[0].position = {h.x, h.y};
        verts[0].color = fill;
        verts[0].tex_coord = {0, 0};

        float startA = h.cutAngle + (h.side > 0 ? 0.0f : kPi);
        for (int i = 0; i < segs; ++i) {
            float frac = static_cast<float>(i) / (segs - 1);
            float a = startA + frac * kPi;
            verts[i + 1].position = {h.x + std::cos(a) * h.radius, h.y + std::sin(a) * h.radius};
            verts[i + 1].color = fill;
            verts[i + 1].tex_coord = {0, 0};

            if (i < segs - 1) {
                idx[i * 3 + 0] = 0;
                idx[i * 3 + 1] = i + 1;
                idx[i * 3 + 2] = i + 2;
            }
        }
        SDL_RenderGeometry(ren_, nullptr, verts.data(), static_cast<int>(verts.size()), idx.data(), static_cast<int>(idx.size()));

        // Borde plano de la cara cortada
        float p1x = h.x + std::cos(startA) * h.radius;
        float p1y = h.y + std::sin(startA) * h.radius;
        float p2x = h.x + std::cos(startA + kPi) * h.radius;
        float p2y = h.y + std::sin(startA + kPi) * h.radius;
        renderThickLine(p1x, p1y, p2x, p2y, 4.0f, CyberTheme::cBlack);
    }
}

void GameRenderer::renderParticles(const std::vector<SparkParticle>& particles) {
    for (const auto& p : particles) {
        drawNeobrutalRect(static_cast<int>(p.x - p.size * 0.5f),
                          static_cast<int>(p.y - p.size * 0.5f),
                          static_cast<int>(p.size), static_cast<int>(p.size),
                          p.color, 2, 2);
    }
}

void GameRenderer::renderTrails(const std::vector<SlashTrailPoint>& trail, SDL_Color col) {
    if (trail.size() < 2) return;

    for (size_t i = 1; i < trail.size(); ++i) {
        const auto& p1 = trail[i - 1];
        const auto& p2 = trail[i];
        float w = (p1.width + p2.width) * 0.5f;
        if (w < 1.0f) continue;

        // Borde negro grueso
        renderThickLine(p1.x, p1.y, p2.x, p2.y, w + 6.0f, CyberTheme::cBlack);
        // Trazo central luminoso del color del jugador
        renderThickLine(p1.x, p1.y, p2.x, p2.y, w, col);
    }
}

void GameRenderer::renderHudSolo(int score, int combo, int lives, float timeLeft) {
    // 1. Caja de Puntaje (Superior Izquierda)
    drawNeobrutalRect(40, 30, 280, 80, CyberTheme::cP1Cyan, 4, 6);
    drawText(fontSmall_, "SCORE", 60, 40, CyberTheme::cBlack);
    drawText(fontHud_, std::to_string(score), 60, 62, CyberTheme::cBlack);

    // 2. Vidas (Corazones / Bloques de Blindaje)
    drawNeobrutalRect(350, 30, 220, 80, CyberTheme::cWhite, 4, 6);
    drawText(fontSmall_, "BLINDAJE", 370, 40, CyberTheme::cBlack);
    for (int l = 0; l < 3; ++l) {
        SDL_Color hpCol = (l < lives) ? CyberTheme::cGreen : CyberTheme::cSilver;
        drawNeobrutalRect(370 + l * 60, 68, 48, 26, hpCol, 3, 2);
    }

    // 3. Combo Activo
    if (combo > 1) {
        drawNeobrutalRect(600, 30, 200, 80, CyberTheme::cYellow, 4, 6);
        drawText(fontSmall_, "COMBO", 620, 40, CyberTheme::cBlack);
        drawText(fontHud_, "x" + std::to_string(combo), 620, 62, CyberTheme::cBlack);
    }

    // 4. Tiempo restante (Superior Derecha)
    char timeBuf[32];
    std::snprintf(timeBuf, sizeof(timeBuf), "TIEMPO: %02d", static_cast<int>(timeLeft));
    drawNeobrutalRect(1920 - 320, 30, 280, 80, CyberTheme::cYellow, 4, 6);
    drawText(fontHud_, timeBuf, 1920 - 180, 52, CyberTheme::cBlack, true);
}

void GameRenderer::renderHud1v1(int p1Score, int p1Combo, int p2Score, int p2Combo, float timeLeft) {
    // P1 (Cian - Izquierda)
    drawNeobrutalRect(40, 30, 340, 90, CyberTheme::cP1Cyan, 4, 6);
    drawText(fontSmall_, "JUGADOR 1 (CIAN)", 60, 40, CyberTheme::cBlack);
    std::string p1Text = "PTS: " + std::to_string(p1Score) + (p1Combo > 1 ? (" (x" + std::to_string(p1Combo) + ")") : "");
    drawText(fontHud_, p1Text, 60, 68, CyberTheme::cBlack);

    // Temporizador Central
    char timeBuf[32];
    std::snprintf(timeBuf, sizeof(timeBuf), "%02d", static_cast<int>(timeLeft));
    drawNeobrutalRect(960 - 75, 30, 150, 90, CyberTheme::cYellow, 4, 6);
    drawText(fontSmall_, "TIEMPO", 960, 40, CyberTheme::cBlack, true);
    drawText(fontTitle_, timeBuf, 960, 62, CyberTheme::cBlack, true);

    // P2 (Rosa - Derecha)
    drawNeobrutalRect(1920 - 380, 30, 340, 90, CyberTheme::cP2Pink, 4, 6);
    drawText(fontSmall_, "JUGADOR 2 (ROSA)", 1920 - 360, 40, CyberTheme::cBlack);
    std::string p2Text = "PTS: " + std::to_string(p2Score) + (p2Combo > 1 ? (" (x" + std::to_string(p2Combo) + ")") : "");
    drawText(fontHud_, p2Text, 1920 - 360, 68, CyberTheme::cBlack);
}

void GameRenderer::renderFloatingScores(const std::vector<FloatingScore>& scores) {
    for (const auto& fs : scores) {
        SDL_Color col = {static_cast<Uint8>(fs.r), static_cast<Uint8>(fs.g), static_cast<Uint8>(fs.b), 255};
        drawText(fontSmall_, fs.text, static_cast<int>(fs.x), static_cast<int>(fs.y), col, true);
    }
}
