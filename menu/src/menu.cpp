#include "menu.h"

#if defined(HAVE_SDL2_IMAGE)
#include <SDL_image.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
// Función de easing cúbico suave (Ease-Out Cubic): desacelera al final
inline float easeOutCubic(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    float inv = 1.0f - t;
    return 1.0f - (inv * inv * inv);
}

// Función Ease-Out-Back: rebota ligeramente al llegar a destino (ideal para el golpe del título)
inline float easeOutBack(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    float p = t - 1.0f;
    return 1.0f + c3 * p * p * p + c1 * p * p;
}
} // namespace

Menu::Menu(SDL_Renderer* renderer, TTF_Font* font, const std::vector<GameEntry>& games)
    : renderer_(renderer), font_(font), games_(games) {
    openController();
    hoverValues_.resize(games.size(), 0.0f);
    particles_.resize(INTRO_MAX_PARTICLES);

    // 1. Inicializar audio SDL_mixer de forma segura
    initAudio();

    // 2. Cargar marca de agua ITF para la esquina superior derecha del menú
    const char* watermarkPaths[] = {
        "assets/sponsors/itf_logo.png",
        "assets/itf_logo.png",
        "assets/itf_logo.bmp",
        "../assets/sponsors/itf_logo.png",
        "../assets/itf_logo.bmp",
        "../../assets/sponsors/itf_logo.png",
        "../../assets/itf_logo.bmp",
        "/home/santino/proyectos/trabajo expo/assets/itf_logo.png",
        "/home/santino/proyectos/trabajo expo/assets/itf_logo.bmp"
    };
    for (const char* p : watermarkPaths) {
        SDL_Surface* surf = loadSurfaceSafe(p);
        if (surf) {
            watermarkTex_ = SDL_CreateTextureFromSurface(renderer_, surf);
            SDL_FreeSurface(surf);
            if (watermarkTex_) {
                SDL_SetTextureBlendMode(watermarkTex_, SDL_BLENDMODE_BLEND);
                SDL_SetTextureAlphaMod(watermarkTex_, 75);
                break;
            }
        }
    }

    // 3. Configurar e inicializar la lista extensible de Splash Screens de Sponsors
    initSponsorSlides();
}

Menu::~Menu() {
    closeController();
    for (auto& pair : textCache_) {
        if (pair.second) {
            SDL_DestroyTexture(pair.second);
        }
    }
    textCache_.clear();

    if (watermarkTex_) {
        SDL_DestroyTexture(watermarkTex_);
        watermarkTex_ = nullptr;
    }

    freeSponsorSlides();
    closeAudio();
}

// ============================================================================
// CARGA DE IMÁGENES (PNG CON SDL_IMAGE Y FALLBACK A BMP) Y SLIDES DE SPONSORS
// ============================================================================

SDL_Surface* Menu::loadSurfaceSafe(const std::string& path) {
    SDL_Surface* surf = nullptr;
#if defined(HAVE_SDL2_IMAGE)
    surf = IMG_Load(path.c_str());
#endif
    if (!surf) {
        // Fallback por si el archivo es un BMP estándar y SDL_image falló o no está activo
        surf = SDL_LoadBMP(path.c_str());
    }
    return surf;
}

void Menu::initSponsorSlides() {
    freeSponsorSlides();

    // Lista configurable de slides. Para agregar más sponsors en el futuro,
    // basta con hacer push_back de un nuevo elemento SponsorSlide aquí.
    slides_ = {
        // Slide 0: PANTALLA 1 – Sponsor ITF (Fondo blanco sólido #FFFFFF)
        {
            "Sponsor ITF",
            {
                "assets/sponsors/itf_logo.png",
                "assets/itf_logo.png",
                "assets/sponsors/itf_logo.bmp",
                "assets/itf_logo.bmp",
                "../assets/sponsors/itf_logo.png",
                "../../assets/sponsors/itf_logo.png"
            },
            SPLASH1_BG_COLOR,
            SPLASH_LOGO_MAX_W_RATIO,
            SPLASH_LOGO_MAX_H_RATIO
        },
        // Slide 1: PANTALLA 2 – Mi Logo (Color de fondo en constante SPLASH2_BG_COLOR)
        {
            "Mi Logo",
            {
                "assets/sponsors/mi_logo.png",
                "assets/sponsors/logo.png",
                "assets/mi_logo.png",
                "assets/logo.png",
                "assets/sponsors/mi_logo.bmp",
                "../assets/sponsors/mi_logo.png",
                "../../assets/sponsors/mi_logo.png"
            },
            SPLASH2_BG_COLOR,
            SPLASH_LOGO_MAX_W_RATIO,
            SPLASH_LOGO_MAX_H_RATIO
        }
    };

    for (auto& slide : slides_) {
        for (const auto& candidatePath : slide.imagePaths) {
            SDL_Surface* surf = loadSurfaceSafe(candidatePath);
            if (surf) {
                slide.texture = SDL_CreateTextureFromSurface(renderer_, surf);
                slide.texW = surf->w;
                slide.texH = surf->h;
                SDL_FreeSurface(surf);
                if (slide.texture) {
                    SDL_SetTextureBlendMode(slide.texture, SDL_BLENDMODE_BLEND);
                    slide.loaded = true;
                    break;
                }
            }
        }
        if (!slide.loaded) {
            std::fprintf(stderr, "[Menu Splash] Aviso: No se pudo cargar la imagen para '%s'. Se saltara este slide.\n",
                         slide.name.c_str());
        }
    }

    // Posicionar en el primer slide válido o pasar directo a INTRO_EPICA si no hay ninguno
    currentSlideIndex_ = 0;
    slideTimer_ = 0.0f;
    while (currentSlideIndex_ < slides_.size() && !slides_[currentSlideIndex_].loaded) {
        currentSlideIndex_++;
    }

    if (currentSlideIndex_ >= slides_.size()) {
        state_ = MenuState::INTRO_EPICA;
        introTimer_ = 0.0f;
    } else {
        state_ = (currentSlideIndex_ == 0) ? MenuState::SPLASH_ITF : MenuState::SPLASH_LOGO;
    }
}

void Menu::freeSponsorSlides() {
    for (auto& slide : slides_) {
        if (slide.texture) {
            SDL_DestroyTexture(slide.texture);
            slide.texture = nullptr;
        }
        slide.loaded = false;
    }
    slides_.clear();
}

void Menu::advanceSponsorSlide() {
    currentSlideIndex_++;
    slideTimer_ = 0.0f;

    // Si algún slide intermedio no tiene imagen cargada, saltarlo automáticamente
    while (currentSlideIndex_ < slides_.size() && !slides_[currentSlideIndex_].loaded) {
        currentSlideIndex_++;
    }

    if (currentSlideIndex_ >= slides_.size()) {
        // Terminaron todos los splashes -> Comienza la Animación Épica de Intro
        state_ = MenuState::INTRO_EPICA;
        introTimer_ = 0.0f;
        hitTriggered_ = false;
        flashFramesRemaining_ = 0;
        shakeTimer_ = 0.0f;
    } else {
        state_ = (currentSlideIndex_ == 0) ? MenuState::SPLASH_ITF : MenuState::SPLASH_LOGO;
    }
}

// ============================================================================
// SISTEMA DE AUDIO CON SDL_MIXER (MÚSICA EN BUCLE, FADE IN/OUT Y MUTE)
// ============================================================================

void Menu::initAudio() {
    if (audioInitialized_) return;

    // Inicializar soporte para OGG (y MP3 si estuviera disponible) sin fallar si no está compilado el codec
    int mixFlags = MIX_INIT_OGG | MIX_INIT_MP3;
    int initted = Mix_Init(mixFlags);
    if ((initted & MIX_INIT_OGG) == 0) {
        std::fprintf(stderr, "[Menu Audio] Aviso: Soporte OGG nativo no reportado por Mix_Init (%s), intentando abrir audio igual.\n",
                     Mix_GetError());
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::fprintf(stderr, "[Menu Audio] Aviso: No se pudo abrir el dispositivo de audio (%s). El menu funcionara sin sonido.\n",
                     Mix_GetError());
        return;
    }

    audioInitialized_ = true;
    Mix_AllocateChannels(16);

    // Ajustar volumen base (70% por defecto)
    int targetVol = static_cast<int>(MIX_MAX_VOLUME * MENU_MUSIC_VOLUME_PCT);
    Mix_VolumeMusic(isMuted_ ? 0 : targetVol);
    Mix_Volume(-1, isMuted_ ? 0 : targetVol);

    // Buscar y cargar música del menú (soporta .ogg, .mp3 o .wav)
    const char* musicCandidates[] = {
        MENU_MUSIC_PATH,
        "assets/audio/menu.mp3",
        "assets/audio/menu_loop.wav",
        "../assets/audio/menu.ogg",
        "../../assets/audio/menu.ogg"
    };
    for (const char* path : musicCandidates) {
        menuMusic_ = Mix_LoadMUS(path);
        if (menuMusic_) break;
    }
    if (!menuMusic_) {
        std::fprintf(stderr, "[Menu Audio] Aviso: No se encontro %s (%s). Continuando sin musica de fondo.\n",
                     MENU_MUSIC_PATH, Mix_GetError());
    }

    // Buscar y cargar el efecto de golpe/impacto del título (hit.wav)
    const char* hitCandidates[] = {
        MENU_HIT_SFX_PATH,
        "../assets/audio/hit.wav",
        "../../assets/audio/hit.wav"
    };
    for (const char* path : hitCandidates) {
        hitSfx_ = Mix_LoadWAV(path);
        if (hitSfx_) break;
    }
    if (!hitSfx_) {
        std::fprintf(stderr, "[Menu Audio] Aviso: No se encontro %s (%s). Continuando sin efecto de impacto.\n",
                     MENU_HIT_SFX_PATH, Mix_GetError());
    }
}

void Menu::closeAudio() {
    if (!audioInitialized_) return;

    Mix_HaltMusic();
    Mix_HaltChannel(-1);

    if (hitSfx_) {
        Mix_FreeChunk(hitSfx_);
        hitSfx_ = nullptr;
    }
    if (menuMusic_) {
        Mix_FreeMusic(menuMusic_);
        menuMusic_ = nullptr;
    }

    Mix_CloseAudio();
    Mix_Quit();
    audioInitialized_ = false;
    musicStarted_ = false;
}

void Menu::startMenuMusicWithFadeIn() {
    if (!audioInitialized_ || !menuMusic_) return;
    if (Mix_PlayingMusic()) return;

    int targetVol = static_cast<int>(MIX_MAX_VOLUME * MENU_MUSIC_VOLUME_PCT);
    Mix_VolumeMusic(isMuted_ ? 0 : targetVol);

    if (Mix_FadeInMusic(menuMusic_, -1, MENU_MUSIC_FADE_IN_MS) < 0) {
        std::fprintf(stderr, "[Menu Audio] Aviso: Error al iniciar musica con fade-in: %s\n", Mix_GetError());
    } else {
        musicStarted_ = true;
    }
}

void Menu::triggerIntroHitAudio() {
    if (!audioInitialized_) return;

    // 1. Reproducir efecto de golpe síncrono con el impacto del título
    if (hitSfx_ && !isMuted_) {
        int hitVol = static_cast<int>(MIX_MAX_VOLUME * std::min(1.0f, MENU_MUSIC_VOLUME_PCT + 0.15f));
        Mix_VolumeChunk(hitSfx_, hitVol);
        Mix_PlayChannel(-1, hitSfx_, 0);
    }

    // 2. Asegurar que la música de menú esté sonando en su pico
    if (!musicStarted_) {
        startMenuMusicWithFadeIn();
    }
}

void Menu::toggleMute() {
    isMuted_ = !isMuted_;
    if (!audioInitialized_) return;

    int targetVol = isMuted_ ? 0 : static_cast<int>(MIX_MAX_VOLUME * MENU_MUSIC_VOLUME_PCT);
    Mix_VolumeMusic(targetVol);
    Mix_Volume(-1, targetVol);
}

// ============================================================================
// GESTIÓN DE MANDOS
// ============================================================================

void Menu::openController() {
    if (controller_) return;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            controller_ = SDL_GameControllerOpen(i);
            if (controller_) break;
        }
    }
}

void Menu::closeController() {
    if (controller_) {
        SDL_GameControllerClose(controller_);
        controller_ = nullptr;
    }
}

// ============================================================================
// PARTÍCULAS Y TRANSICIÓN DE LA ANIMACIÓN ÉPICA
// ============================================================================

void Menu::spawnImpactParticles(float centerX, float centerY) {
    for (int i = 0; i < INTRO_MAX_PARTICLES; ++i) {
        IntroParticle& p = particles_[i];
        p.active = true;

        // Distribuir el origen a lo largo del borde de la caja del título (1200x200)
        float perimeterOffset = (static_cast<float>(i) / INTRO_MAX_PARTICLES) * 2.0f * 3.14159265f;
        float spawnRadiusX = 480.0f * std::cos(perimeterOffset);
        float spawnRadiusY = 85.0f * std::sin(perimeterOffset);
        p.x = centerX + spawnRadiusX;
        p.y = centerY + spawnRadiusY;

        // Velocidad radial explosiva hacia afuera con variación pseudoaleatoria
        float speed = 260.0f + static_cast<float>((i * 97) % 620);
        float jitterAngle = perimeterOffset + ((i % 7) - 3) * 0.08f;
        p.vx = std::cos(jitterAngle) * speed;
        p.vy = std::sin(jitterAngle) * speed - 120.0f; // Impulso inicial hacia arriba

        p.size = 8.0f + static_cast<float>((i * 13) % 14);
        p.angle = static_cast<float>((i * 45) % 360);
        p.angularVel = ((i % 2 == 0) ? 1.0f : -1.0f) * (180.0f + (i * 29) % 360);
        p.maxLife = 0.65f + ((i * 31) % 65) * 0.01f;
        p.life = p.maxLife;

        // Alternar colores neobrutalistas (Dorado, Cian, Rosa/Rojo, Blanco, Negro)
        switch (i % 5) {
            case 0: p.color = INTRO_LIGHT_LINE_COL; break;
            case 1: p.color = INTRO_ACCENT_CYAN_COL; break;
            case 2: p.color = INTRO_ACCENT_PINK_COL; break;
            case 3: p.color = SDL_Color{255, 255, 255, 255}; break;
            default: p.color = SDL_Color{18, 18, 18, 255}; break;
        }
    }
}

void Menu::updateImpactParticles(float dt) {
    constexpr float gravity = 680.0f;
    for (auto& p : particles_) {
        if (!p.active) continue;
        p.life -= dt;
        if (p.life <= 0.0f) {
            p.active = false;
            continue;
        }
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.vy += gravity * dt;
        p.angle += p.angularVel * dt;
    }
}

void Menu::skipToPulsaX() {
    state_ = MenuState::PULSA_X;
    pulsaXFadeAlpha_ = 1.0f; // Directamente visible al omitir
    flashFramesRemaining_ = 0;
    shakeTimer_ = 0.0f;
    startMenuMusicWithFadeIn();
}

// ============================================================================
// ACTUALIZACIÓN POR ESTADO (UPDATE CON DELTA TIME A 60 FPS)
// ============================================================================

void Menu::update(float dt) {
    globalTime_ += dt;

    switch (state_) {
        case MenuState::SPLASH_ITF:
        case MenuState::SPLASH_LOGO:
            updateSplash(dt);
            break;

        case MenuState::INTRO_EPICA:
            updateIntroEpica(dt);
            break;

        case MenuState::PULSA_X:
            updatePulsaX(dt);
            break;

        case MenuState::MENU:
            updateMenu(dt);
            break;
    }
}

void Menu::updateSplash(float dt) {
    if (currentSlideIndex_ >= slides_.size()) {
        advanceSponsorSlide();
        return;
    }

    slideTimer_ += dt;
    const float totalSlideDuration = SPLASH_FADE_IN_DURATION + SPLASH_HOLD_DURATION + SPLASH_FADE_OUT_DURATION;

    if (slideTimer_ >= totalSlideDuration) {
        advanceSponsorSlide();
    }
}

void Menu::updateIntroEpica(float dt) {
    float prevTimer = introTimer_;
    introTimer_ += dt;

    // Al entrar en la Fase 2 (1.0 s), comenzamos el fade-in de 1.5 s de la música
    // para que alcance su plenitud justo cuando el título impacta en 2.0 s - 2.3 s
    if (prevTimer < INTRO_PHASE1_END && introTimer_ >= INTRO_PHASE1_END) {
        startMenuMusicWithFadeIn();
    }

    // El título entra desde 3x a 1x entre 2.0 s y 2.35 s, produciendo el impacto síncrono
    constexpr float kImpactMoment = 2.32f;
    if (!hitTriggered_ && introTimer_ >= kImpactMoment) {
        hitTriggered_ = true;
        flashFramesRemaining_ = INTRO_FLASH_FRAMES;
        shakeTimer_ = INTRO_SHAKE_DURATION;
        spawnImpactParticles(LOGICAL_WIDTH * 0.5f, 320.0f);
        triggerIntroHitAudio();
    }

    if (shakeTimer_ > 0.0f) {
        shakeTimer_ = std::max(0.0f, shakeTimer_ - dt);
    }

    updateImpactParticles(dt);

    // Al llegar a los 4.0 s, pasamos suavemente al estado PULSA_X
    if (introTimer_ >= INTRO_PHASE4_END) {
        state_ = MenuState::PULSA_X;
        pulsaXFadeAlpha_ = 0.0f; // Inicia su fade-in suave en el estado PULSA_X
        startMenuMusicWithFadeIn();
    }
}

void Menu::updatePulsaX(float dt) {
    // Seguir actualizando partículas remanentes para que no desaparezcan de golpe
    updateImpactParticles(dt);

    if (pulsaXFadeAlpha_ < 1.0f) {
        pulsaXFadeAlpha_ = std::min(1.0f, pulsaXFadeAlpha_ + dt * 2.2f);
    }
}

void Menu::updateMenu(float dt) {
    // Normalizar velocidad de interpolación a 60 FPS usando dt
    float frameFactor = std::min(1.0f, dt * 60.0f);

    float targetScrollY = 0.0f;
    if (selected_ >= 2) {
        targetScrollY = -(selected_ - 1) * 240.0f;
    }
    scrollY_ += (targetScrollY - scrollY_) * (0.15f * frameFactor);

    for (size_t i = 0; i < games_.size(); ++i) {
        bool sel = (i == static_cast<size_t>(selected_));
        float target = sel ? 1.0f : 0.0f;
        hoverValues_[i] += (target - hoverValues_[i]) * (0.25f * frameFactor);
    }
}

// ============================================================================
// RENDERIZADO DE ELEMENTOS VISUALES Y POR ESTADO
// ============================================================================

void Menu::renderText(const std::string& text, int x, int y, SDL_Color color, float scale, bool center, Uint8 alpha) {
    if (!font_ || text.empty() || alpha == 0) return;

    std::string key = text + "_" + std::to_string(color.r) + "_" + std::to_string(color.g) + "_" + std::to_string(color.b);
    SDL_Texture* tex = nullptr;

    auto it = textCache_.find(key);
    if (it == textCache_.end()) {
        SDL_Surface* surf = TTF_RenderUTF8_Blended(font_, text.c_str(), color);
        if (surf) {
            tex = SDL_CreateTextureFromSurface(renderer_, surf);
            SDL_FreeSurface(surf);
            if (tex) {
                SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
            }
            textCache_[key] = tex;
        }
    } else {
        tex = it->second;
    }

    if (!tex) return;

    SDL_SetTextureAlphaMod(tex, alpha);

    int w = 0, h = 0;
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);

    int dstW = static_cast<int>(w * scale);
    int dstH = static_cast<int>(h * scale);
    int dstX = center ? (x - dstW / 2) : x;
    int dstY = center ? (y - dstH / 2) : y;

    SDL_Rect dst = {dstX, dstY, dstW, dstH};
    SDL_RenderCopy(renderer_, tex, nullptr, &dst);
}

void Menu::renderAnimatedGridBackground(float zoomScale, int offsetX, int offsetY) {
    // Fondo claro base
    SDL_SetRenderDrawColor(renderer_, 250, 250, 250, 255);
    SDL_RenderClear(renderer_);

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 16);

    float step = 60.0f * zoomScale;
    if (step < 10.0f) step = 60.0f;

    float animOffset = std::fmod(globalTime_ * 40.0f * zoomScale, step);
    float centerX = LOGICAL_WIDTH * 0.5f + offsetX;
    float centerY = LOGICAL_HEIGHT * 0.5f + offsetY;

    // Líneas verticales con soporte de zoom desde el centro de la pantalla
    for (float x = centerX + animOffset; x < LOGICAL_WIDTH; x += step) {
        SDL_RenderDrawLine(renderer_, static_cast<int>(x), 0, static_cast<int>(x), LOGICAL_HEIGHT);
    }
    for (float x = centerX + animOffset - step; x >= 0; x -= step) {
        SDL_RenderDrawLine(renderer_, static_cast<int>(x), 0, static_cast<int>(x), LOGICAL_HEIGHT);
    }

    // Líneas horizontales con soporte de zoom desde el centro de la pantalla
    for (float y = centerY + animOffset; y < LOGICAL_HEIGHT; y += step) {
        SDL_RenderDrawLine(renderer_, 0, static_cast<int>(y), LOGICAL_WIDTH, static_cast<int>(y));
    }
    for (float y = centerY + animOffset - step; y >= 0; y -= step) {
        SDL_RenderDrawLine(renderer_, 0, static_cast<int>(y), LOGICAL_WIDTH, static_cast<int>(y));
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
}

void Menu::renderFloatingConfetti(int offsetX, int offsetY) {
    SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 255);
    for (int i = 0; i < 20; i++) {
        float speed = 30.0f + (i % 5) * 15.0f;
        float yPos = 1200.0f - std::fmod((globalTime_ * speed + i * 150.0f), 1400.0f);
        float xPos = std::fmod((i * 373.0f), 1920.0f);
        float wobble = std::sin(globalTime_ * 1.5f + i) * 40.0f;

        int size = 20 + (i % 3) * 15;
        SDL_Rect r = {
            static_cast<int>(xPos + wobble) + offsetX,
            static_cast<int>(yPos) + offsetY,
            size,
            size
        };

        if (i % 2 == 0) {
            SDL_RenderFillRect(renderer_, &r);
        } else {
            for (int b = 0; b < 4; b++) {
                SDL_Rect br = {r.x + b, r.y + b, r.w - b * 2, r.h - b * 2};
                SDL_RenderDrawRect(renderer_, &br);
            }
        }
    }
}

void Menu::renderSunburstRays(int centerX, int centerY, float rotationRad, float alphaNorm) {
    if (alphaNorm <= 0.0f) return;

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    Uint8 rayAlpha = static_cast<Uint8>(std::clamp(alphaNorm * 95.0f, 0.0f, 255.0f));

    // Dibujar rayos radiales estilo arcade neobrutalista rotando lentamente detrás del título
    for (int i = 0; i < INTRO_SUNBURST_RAYS; ++i) {
        float baseAngle = rotationRad + (static_cast<float>(i) * 2.0f * 3.14159265f / INTRO_SUNBURST_RAYS);
        SDL_Color col = (i % 2 == 0) ? INTRO_LIGHT_LINE_COL : INTRO_ACCENT_CYAN_COL;
        SDL_SetRenderDrawColor(renderer_, col.r, col.g, col.b, rayAlpha);

        // Cada rayo se dibuja como un haz grueso de líneas divergentes
        for (int spread = -10; spread <= 10; ++spread) {
            float a = baseAngle + spread * 0.0065f;
            int x1 = centerX + static_cast<int>(std::cos(a) * 90.0f);
            int y1 = centerY + static_cast<int>(std::sin(a) * 50.0f);
            int x2 = centerX + static_cast<int>(std::cos(a) * 860.0f);
            int y2 = centerY + static_cast<int>(std::sin(a) * 460.0f);
            SDL_RenderDrawLine(renderer_, x1, y1, x2, y2);
        }
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
}

void Menu::renderTitleBanner(float scale, int offsetX, int offsetY, float shineProgress) {
    int baseW = 1200;
    int baseH = 200;
    int titleW = static_cast<int>(baseW * scale);
    int titleH = static_cast<int>(baseH * scale);

    int centerX = LOGICAL_WIDTH / 2 + offsetX;
    int centerY = 320 + offsetY;
    int titleX = centerX - titleW / 2;
    int titleY = centerY - titleH / 2;

    int shadowOffsetX = static_cast<int>((16 + std::cos(globalTime_ * 3.0f) * 6.0f) * scale);
    int shadowOffsetY = static_cast<int>((16 + std::sin(globalTime_ * 3.0f) * 6.0f) * scale);

    // Sombra dura neobrutalista
    SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 255);
    SDL_Rect titleShadow = {titleX + shadowOffsetX, titleY + shadowOffsetY, titleW, titleH};
    SDL_RenderFillRect(renderer_, &titleShadow);

    // Caja blanca principal del título
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_Rect titleBox = {titleX, titleY, titleW, titleH};
    SDL_RenderFillRect(renderer_, &titleBox);

    // Texto principal "DUAL SHOW"
    renderText("DUAL SHOW", centerX, centerY, SDL_Color{10, 10, 10, 255}, 1.8f * scale, true);

    // Efecto "Shine Sweep" (brillo diagonal/vertical que recorre las letras y la caja)
    if (shineProgress > 0.0f && shineProgress < 1.0f) {
        SDL_RenderSetClipRect(renderer_, &titleBox);
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

        int sweepCenterX = titleX - 180 + static_cast<int>((titleW + 360) * shineProgress);
        // Banda luminosa dorada/blanca que ilumina las letras a su paso
        SDL_SetRenderDrawColor(renderer_, 250, 204, 21, 140);
        SDL_Rect glowBand = {sweepCenterX - 65, titleY, 130, titleH};
        SDL_RenderFillRect(renderer_, &glowBand);

        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 215);
        SDL_Rect coreBand = {sweepCenterX - 24, titleY, 48, titleH};
        SDL_RenderFillRect(renderer_, &coreBand);

        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
        SDL_RenderSetClipRect(renderer_, nullptr);
    }

    // Borde grueso negro perimetral
    SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 255);
    int borderThickness = std::max(4, static_cast<int>(8 * scale));
    for (int b = 0; b < borderThickness; ++b) {
        SDL_Rect bR = {titleX + b, titleY + b, titleW - b * 2, titleH - b * 2};
        SDL_RenderDrawRect(renderer_, &bR);
    }
}

void Menu::renderWatermark(Uint8 alpha) {
    if (!watermarkTex_) return;
    SDL_SetTextureAlphaMod(watermarkTex_, alpha);
    SDL_Rect markRect = {LOGICAL_WIDTH - 240, 36, 190, 147};
    SDL_RenderCopy(renderer_, watermarkTex_, nullptr, &markRect);
}

void Menu::renderMuteIndicator() {
    if (!isMuted_) return;
    // Etiqueta discreta neobrutalista cuando el audio está silenciado con [M]
    SDL_Rect muteBg = {40, 36, 290, 44};
    SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 255);
    SDL_Rect muteShadow = {muteBg.x + 4, muteBg.y + 4, muteBg.w, muteBg.h};
    SDL_RenderFillRect(renderer_, &muteShadow);

    SDL_SetRenderDrawColor(renderer_, 244, 63, 94, 255);
    SDL_RenderFillRect(renderer_, &muteBg);
    renderText("AUDIO SILENCIADO [M]", muteBg.x + muteBg.w / 2, muteBg.y + muteBg.h / 2,
               SDL_Color{255, 255, 255, 255}, 0.26f, true);
}

void Menu::render() {
    switch (state_) {
        case MenuState::SPLASH_ITF:
        case MenuState::SPLASH_LOGO:
            renderSplash();
            break;

        case MenuState::INTRO_EPICA:
            renderIntroEpica();
            break;

        case MenuState::PULSA_X:
            renderPulsaX();
            break;

        case MenuState::MENU:
            renderMenu();
            break;
    }

    SDL_RenderPresent(renderer_);
}

void Menu::renderSplash() {
    if (currentSlideIndex_ >= slides_.size()) return;
    const SponsorSlide& slide = slides_[currentSlideIndex_];

    // 1. Limpiar con el color de fondo configurado para este Splash
    SDL_SetRenderDrawColor(renderer_, slide.bgColor.r, slide.bgColor.g, slide.bgColor.b, 255);
    SDL_RenderClear(renderer_);

    // 2. Calcular factor de opacidad (Fade in 0.6 s -> Permanece 2.0 s -> Fade out 0.6 s)
    float alphaNorm = 1.0f;
    if (slideTimer_ < SPLASH_FADE_IN_DURATION) {
        alphaNorm = slideTimer_ / SPLASH_FADE_IN_DURATION;
    } else if (slideTimer_ > SPLASH_FADE_IN_DURATION + SPLASH_HOLD_DURATION) {
        float fadeOutElapsed = slideTimer_ - (SPLASH_FADE_IN_DURATION + SPLASH_HOLD_DURATION);
        alphaNorm = 1.0f - (fadeOutElapsed / SPLASH_FADE_OUT_DURATION);
    }
    alphaNorm = std::clamp(alphaNorm, 0.0f, 1.0f);
    Uint8 alphaByte = static_cast<Uint8>(alphaNorm * 255.0f);

    // 3. Dibujar logo centrado (~55% del ancho de ventana, manteniendo proporción sin deformar)
    if (slide.texture && slide.texW > 0 && slide.texH > 0) {
        float maxW = LOGICAL_WIDTH * slide.maxWidthRatio;
        float maxH = LOGICAL_HEIGHT * slide.maxHeightRatio;

        float scaleW = maxW / static_cast<float>(slide.texW);
        float scaleH = maxH / static_cast<float>(slide.texH);
        float scale = std::min(scaleW, scaleH);

        // Ligero efecto de respiración/zoom suave (0.97 -> 1.0) durante el splash
        float subtleZoom = 0.97f + 0.03f * easeOutCubic(std::min(1.0f, slideTimer_ / (SPLASH_FADE_IN_DURATION + SPLASH_HOLD_DURATION)));
        scale *= subtleZoom;

        int dstW = static_cast<int>(slide.texW * scale);
        int dstH = static_cast<int>(slide.texH * scale);
        int dstX = (LOGICAL_WIDTH - dstW) / 2;
        int dstY = (LOGICAL_HEIGHT - dstH) / 2;

        SDL_SetTextureAlphaMod(slide.texture, alphaByte);
        SDL_Rect dstRect = {dstX, dstY, dstW, dstH};
        SDL_RenderCopy(renderer_, slide.texture, nullptr, &dstRect);
    }
}

void Menu::renderIntroEpica() {
    // Calcular desplazamiento de cámara por Screen Shake (amplitud ~8 px, decae en 0.3 s)
    int shakeX = 0;
    int shakeY = 0;
    if (shakeTimer_ > 0.0f) {
        float decay = shakeTimer_ / INTRO_SHAKE_DURATION;
        float amp = INTRO_SHAKE_AMPLITUDE * decay;
        shakeX = static_cast<int>(std::sin(introTimer_ * 145.0f) * amp);
        shakeY = static_cast<int>(std::cos(introTimer_ * 175.0f) * amp);
    }

    // ========================================================================
    // FASE 1 (0.0 - 1.0 s): Pantalla oscura + línea de luz fina horizontal
    // ========================================================================
    if (introTimer_ < INTRO_PHASE1_END) {
        SDL_SetRenderDrawColor(renderer_, INTRO_DARK_BG_COLOR.r, INTRO_DARK_BG_COLOR.g, INTRO_DARK_BG_COLOR.b, 255);
        SDL_RenderClear(renderer_);

        float p1 = easeOutCubic(introTimer_ / INTRO_PHASE1_END);
        int lineW = static_cast<int>(LOGICAL_WIDTH * p1);
        int lineX = (LOGICAL_WIDTH - lineW) / 2;
        int centerY = LOGICAL_HEIGHT / 2;

        // Halo exterior brillante
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer_, INTRO_LIGHT_LINE_COL.r, INTRO_LIGHT_LINE_COL.g, INTRO_LIGHT_LINE_COL.b, 95);
        SDL_Rect glowRect = {lineX, centerY - 8, lineW, 16};
        SDL_RenderFillRect(renderer_, &glowRect);

        // Núcleo fino de luz intensa
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        SDL_Rect coreRect = {lineX, centerY - 2, lineW, 4};
        SDL_RenderFillRect(renderer_, &coreRect);

        // Destello central pulsante (latido)
        float pulse = 0.5f + 0.5f * std::sin(introTimer_ * 18.0f);
        int sparkW = static_cast<int>(40 + 80 * pulse);
        SDL_SetRenderDrawColor(renderer_, INTRO_ACCENT_CYAN_COL.r, INTRO_ACCENT_CYAN_COL.g, INTRO_ACCENT_CYAN_COL.b, 180);
        SDL_Rect centerSpark = {LOGICAL_WIDTH / 2 - sparkW / 2, centerY - 5, sparkW, 10};
        SDL_RenderFillRect(renderer_, &centerSpark);
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
        return;
    }

    // ========================================================================
    // FASE 2 (1.0 - 2.0 s): Wipe de barras gruesas neobrutalistas + Zoom lento
    // ========================================================================
    float p2Raw = (introTimer_ - INTRO_PHASE1_END) / (INTRO_PHASE2_END - INTRO_PHASE1_END);
    float p2 = easeOutCubic(std::clamp(p2Raw, 0.0f, 1.0f));
    float currentZoom = INTRO_BG_ZOOM_START + (1.0f - INTRO_BG_ZOOM_START) * p2;

    // Dibujar fondo del menú con la escala de zoom actual (1.15 -> 1.0)
    renderAnimatedGridBackground(currentZoom, shakeX, shakeY);
    renderFloatingConfetti(shakeX, shakeY);

    if (introTimer_ < INTRO_PHASE2_END) {
        // Revelar mediante barras horizontales escalonadas que se abren desde el centro
        int barH = (LOGICAL_HEIGHT + INTRO_WIPE_BARS - 1) / INTRO_WIPE_BARS;
        int centerBar = INTRO_WIPE_BARS / 2;

        for (int i = 0; i < INTRO_WIPE_BARS; ++i) {
            float distFromCenter = static_cast<float>(std::abs(i - centerBar)) / static_cast<float>(centerBar + 1);
            float localT = std::clamp((p2Raw - distFromCenter * 0.32f) / 0.68f, 0.0f, 1.0f);
            float openFrac = easeOutCubic(localT);

            // Las cortinas oscuras se retiran hacia los bordes con un borde grueso de color neobrutalista
            int remainingW = static_cast<int>((LOGICAL_WIDTH * 0.5f) * (1.0f - openFrac));
            int y = i * barH;

            if (remainingW > 0) {
                SDL_SetRenderDrawColor(renderer_, INTRO_DARK_BG_COLOR.r, INTRO_DARK_BG_COLOR.g, INTRO_DARK_BG_COLOR.b, 255);
                SDL_Rect leftShutter = {0, y, remainingW, barH + 1};
                SDL_Rect rightShutter = {LOGICAL_WIDTH - remainingW, y, remainingW, barH + 1};
                SDL_RenderFillRect(renderer_, &leftShutter);
                SDL_RenderFillRect(renderer_, &rightShutter);

                // Borde líder neobrutalista en las barras
                SDL_Color edgeCol = (i % 2 == 0) ? INTRO_LIGHT_LINE_COL : INTRO_ACCENT_CYAN_COL;
                SDL_SetRenderDrawColor(renderer_, edgeCol.r, edgeCol.g, edgeCol.b, 255);
                SDL_Rect leftEdge = {remainingW - 14, y, 14, barH + 1};
                SDL_Rect rightEdge = {LOGICAL_WIDTH - remainingW, y, 14, barH + 1};
                SDL_RenderFillRect(renderer_, &leftEdge);
                SDL_RenderFillRect(renderer_, &rightEdge);
            }
        }
        return;
    }

    // ========================================================================
    // FASES 3 Y 4 (2.0 - 4.0 s): Golpe del título (3x -> 1x), Rayos, Partículas y Shine Sweep
    // ========================================================================

    // Rayos de luz radiales girando lento detrás del título (aparecen tras el impacto)
    if (hitTriggered_) {
        float rayFade = std::clamp((introTimer_ - 2.32f) / 0.45f, 0.0f, 1.0f);
        // En la recta final hacia PULSA_X suavizamos los rayos para empalmar limpio
        if (introTimer_ > 3.5f) {
            rayFade *= std::clamp(1.0f - (introTimer_ - 3.5f) / 0.5f, 0.25f, 1.0f);
        }
        renderSunburstRays(LOGICAL_WIDTH / 2 + shakeX, 320 + shakeY, globalTime_ * 0.35f, rayFade);
    }

    // Escala del título entrando con golpe (3x -> 1x con ease-out-back en los primeros 0.32s de Fase 3)
    float slamElapsed = introTimer_ - INTRO_PHASE2_END;
    float slamT = std::clamp(slamElapsed / 0.32f, 0.0f, 1.0f);
    float titleScale = INTRO_TITLE_SCALE_START + (1.0f - INTRO_TITLE_SCALE_START) * easeOutBack(slamT);

    // Progreso del brillo que recorre las letras (Shine Sweep entre 3.0 s y 3.85 s)
    float shineProgress = -1.0f;
    if (introTimer_ >= INTRO_PHASE3_END) {
        shineProgress = (introTimer_ - INTRO_PHASE3_END) / 0.85f;
    }

    renderTitleBanner(titleScale, shakeX, shakeY, shineProgress);

    // Subtítulo aparece suavemente tras el golpe
    if (introTimer_ >= 2.45f) {
        float subAlphaNorm = std::clamp((introTimer_ - 2.45f) / 0.4f, 0.0f, 1.0f);
        renderText("EXPERIENCIA SENSORIAL INTERACTIVA",
                   LOGICAL_WIDTH / 2 + shakeX, 480 + shakeY,
                   SDL_Color{80, 80, 80, 255}, 0.45f, true,
                   static_cast<Uint8>(subAlphaNorm * 255.0f));
    }

    // Dibujar chispas/partículas neobrutalistas saliendo del punto de impacto
    for (const auto& p : particles_) {
        if (!p.active) continue;
        float lifeRatio = std::clamp(p.life / p.maxLife, 0.0f, 1.0f);
        int sz = std::max(4, static_cast<int>(p.size * lifeRatio));
        int px = static_cast<int>(p.x) - sz / 2 + shakeX;
        int py = static_cast<int>(p.y) - sz / 2 + shakeY;

        // Sombra negra de la chispa
        SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 255);
        SDL_Rect pShadow = {px + 3, py + 3, sz, sz};
        SDL_RenderFillRect(renderer_, &pShadow);

        // Cuerpo de color de la chispa
        SDL_SetRenderDrawColor(renderer_, p.color.r, p.color.g, p.color.b, 255);
        SDL_Rect pRect = {px, py, sz, sz};
        SDL_RenderFillRect(renderer_, &pRect);
    }

    // Flash blanco de 2-3 frames exactos en el momento del impacto
    if (flashFramesRemaining_ > 0) {
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        Uint8 flashAlpha = (flashFramesRemaining_ == INTRO_FLASH_FRAMES) ? 235 : 140;
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, flashAlpha);
        SDL_Rect fullScreen = {0, 0, LOGICAL_WIDTH, LOGICAL_HEIGHT};
        SDL_RenderFillRect(renderer_, &fullScreen);
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
        flashFramesRemaining_--;
    }
}

void Menu::renderPulsaX() {
    // 1. Fondo cuadriculado animado y confeti brutalista
    renderAnimatedGridBackground(1.0f, 0, 0);
    renderFloatingConfetti(0, 0);

    // 2. Rayos sutiles girando detrás del título
    renderSunburstRays(LOGICAL_WIDTH / 2, 320, globalTime_ * 0.25f, 0.25f);

    // 3. Brillo periódico suave en las letras cada 5 segundos
    float periodicShine = std::fmod(globalTime_, 5.0f);
    float shineProg = (periodicShine < 0.85f) ? (periodicShine / 0.85f) : -1.0f;

    // 4. Título principal con caja brutalista gigante
    renderTitleBanner(1.0f, 0, 0, shineProg);
    renderText("EXPERIENCIA SENSORIAL INTERACTIVA", LOGICAL_WIDTH / 2, 480, SDL_Color{80, 80, 80, 255}, 0.45f, true);

    // 5. Partículas remanentes si acaba de terminar la intro
    for (const auto& p : particles_) {
        if (!p.active) continue;
        float lifeRatio = std::clamp(p.life / p.maxLife, 0.0f, 1.0f);
        int sz = std::max(4, static_cast<int>(p.size * lifeRatio));
        SDL_SetRenderDrawColor(renderer_, p.color.r, p.color.g, p.color.b, 255);
        SDL_Rect pRect = {static_cast<int>(p.x) - sz / 2, static_cast<int>(p.y) - sz / 2, sz, sz};
        SDL_RenderFillRect(renderer_, &pRect);
    }

    // 6. Botón "PULSA [X] PARA INICIAR" con fade-in y pulso suave (escala 1.0 <-> 1.05) en bucle
    Uint8 fadeAlpha = static_cast<Uint8>(std::clamp(pulsaXFadeAlpha_, 0.0f, 1.0f) * 255.0f);
    float pulse = (std::sin(globalTime_ * 6.0f) + 1.0f) * 0.5f;
    float btnScale = 1.0f + 0.05f * pulse; // Escala 1.0 <-> 1.05

    int baseW = 1000;
    int baseH = 130;
    int btnW = static_cast<int>(baseW * btnScale);
    int btnH = static_cast<int>(baseH * btnScale);
    int btnX = LOGICAL_WIDTH / 2 - btnW / 2;
    int btnY = 680 + static_cast<int>(std::sin(globalTime_ * 2.5f) * 12.0f);

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(renderer_, 10, 10, 10, fadeAlpha);
    SDL_Rect btnShadowRect = {btnX + 12, btnY + 12, btnW, btnH};
    SDL_RenderFillRect(renderer_, &btnShadowRect);

    int bgCol = 250 - static_cast<int>(25.0f * pulse);
    SDL_SetRenderDrawColor(renderer_, bgCol, bgCol, bgCol, fadeAlpha);
    SDL_Rect btnRect = {btnX, btnY, btnW, btnH};
    SDL_RenderFillRect(renderer_, &btnRect);

    SDL_SetRenderDrawColor(renderer_, 10, 10, 10, fadeAlpha);
    for (int b = 0; b < 6; ++b) {
        SDL_Rect bRect = {btnX + b, btnY + b, btnW - b * 2, btnH - b * 2};
        SDL_RenderDrawRect(renderer_, &bRect);
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);

    float textScale = 0.65f * btnScale;
    renderText("PULSA [X] PARA INICIAR", LOGICAL_WIDTH / 2, btnY + btnH / 2,
               SDL_Color{10, 10, 10, 255}, textScale, true, fadeAlpha);

    // 7. Marca de agua ITF arriba a la derecha e indicador de silencio
    renderWatermark(75);
    renderMuteIndicator();
}

void Menu::renderMenu() {
    // 1. Fondo cuadriculado animado
    renderAnimatedGridBackground(1.0f, 0, 0);

    // 2. Título masivo y moderno
    renderText("DUAL SHOW", 120, 80, SDL_Color{10, 10, 10, 255}, 1.0f);
    renderText("SELECCIONA UN PROYECTO", 130, 220, SDL_Color{100, 100, 100, 255}, 0.4f);

    // 3. Marca de agua arriba a la derecha e indicador de mute
    renderWatermark(75);
    renderMuteIndicator();

    // 4. Línea gruesa debajo del título
    SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 255);
    SDL_Rect titleLine = {120, 280, 1600, 8};
    SDL_RenderFillRect(renderer_, &titleLine);

    // 5. Clip Rect para las tarjetas de juegos
    SDL_Rect clipRect = {0, 288, LOGICAL_WIDTH, 712};
    SDL_RenderSetClipRect(renderer_, &clipRect);

    int baseY = 340;
    for (size_t i = 0; i < games_.size(); ++i) {
        bool sel = (i == static_cast<size_t>(selected_));
        float hv = hoverValues_[i];

        int offsetX = (i % 2 == 0) ? 0 : 60;
        int hoverOffset = static_cast<int>(-28.0f * hv);

        int x = 240 + offsetX;
        int y = baseY + hoverOffset + static_cast<int>(scrollY_);
        int w = 1100;
        int h = 210;

        if (y + h < 280 || y > 1000) {
            baseY += 240;
            continue;
        }

        // Sombra dura sólida negra
        int shadowOffset = 16 + static_cast<int>(20.0f * hv);
        SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 255);
        SDL_Rect shadow = {x + shadowOffset, y + shadowOffset, w, h};
        SDL_RenderFillRect(renderer_, &shadow);

        // Fondo de la tarjeta
        int cVal = 230 + static_cast<int>(25.0f * hv);
        SDL_SetRenderDrawColor(renderer_, cVal, cVal, cVal, 255);
        SDL_Rect card = {x, y, w, h};
        SDL_RenderFillRect(renderer_, &card);

        // Línea divisoria superior
        SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 255);
        SDL_Rect headerLine = {x, y + 50, w, 6};
        SDL_RenderFillRect(renderer_, &headerLine);

        // Botones decorativos de ventana
        SDL_Rect btn1 = {x + 30, y + 16, 20, 20};
        SDL_Rect btn2 = {x + 65, y + 16, 20, 20};
        SDL_RenderFillRect(renderer_, &btn1);
        SDL_RenderFillRect(renderer_, &btn2);

        // Borde grueso negro perimetral
        for (int b = 0; b < 6; ++b) {
            SDL_Rect border = {x + b, y + b, w - b * 2, h - b * 2};
            SDL_RenderDrawRect(renderer_, &border);
        }

        // Textos del juego
        renderText(games_[i].name, x + 60, y + 75, SDL_Color{10, 10, 10, 255}, 0.7f);
        if (!games_[i].description.empty()) {
            renderText(games_[i].description, x + 60, y + 145, SDL_Color{80, 80, 80, 255}, 0.4f);
        }

        // Flecha interactiva a la izquierda
        if (sel) {
            int arrowX = static_cast<int>(x - 140 + (30.0f * hv));
            renderText("->", arrowX, y + 80, SDL_Color{10, 10, 10, 255}, 0.8f);
        }

        baseY += 240;
    }

    SDL_RenderSetClipRect(renderer_, nullptr);

    if (games_.empty()) {
        renderText("No hay juegos registrados.", 120, 440, SDL_Color{10, 10, 10, 255}, 0.5f);
    }

    if (!statusMessage_.empty() && SDL_GetTicks() < statusUntil_) {
        renderText(statusMessage_, 240, 950, SDL_Color{10, 10, 10, 255}, 0.5f);
    }

    // Footer (Barra inferior con controles + tecla M de silencio)
    SDL_SetRenderDrawColor(renderer_, 250, 250, 250, 255);
    SDL_Rect footerBg = {0, 1000, LOGICAL_WIDTH, 80};
    SDL_RenderFillRect(renderer_, &footerBg);

    SDL_SetRenderDrawColor(renderer_, 10, 10, 10, 255);
    SDL_Rect footerLine = {0, 1000, LOGICAL_WIDTH, 8};
    SDL_RenderFillRect(renderer_, &footerLine);

    renderText("Arriba/Abajo: Navegar  |  X (A): Confirmar  |  M: Silenciar Musica  |  Circulo (B) / ESC: Salir",
               40, 1024, SDL_Color{30, 30, 30, 255}, 0.36f);
}

// ============================================================================
// NAVEGACIÓN, LANZAMIENTO DE MINIJUEGOS Y MANEJO DE EVENTOS
// ============================================================================

void Menu::moveUp() {
    if (!games_.empty()) {
        selected_ = (selected_ - 1 + static_cast<int>(games_.size())) % static_cast<int>(games_.size());
    }
}

void Menu::moveDown() {
    if (!games_.empty()) {
        selected_ = (selected_ + 1) % static_cast<int>(games_.size());
    }
}

void Menu::launchSelected() {
    if (games_.empty()) return;
    const GameEntry& g = games_[static_cast<size_t>(selected_)];
    if (g.executable.empty()) return;

    // 1. Bajar la música con fade out y liberar el dispositivo de audio para que no choque con el minijuego
    if (audioInitialized_) {
        if (Mix_PlayingMusic()) {
            Mix_FadeOutMusic(MENU_MUSIC_FADE_OUT_MS);
            Uint32 waitStart = SDL_GetTicks();
            while (Mix_PlayingMusic() && (SDL_GetTicks() - waitStart) < static_cast<Uint32>(MENU_MUSIC_FADE_OUT_MS + 40)) {
                SDL_Delay(10);
            }
        }
        closeAudio();
    }

    // 2. Liberar el mando para que el minijuego hijo pueda usarlo
    closeController();

    // 3. Ejecutar el minijuego
    int code = launcher_.launch(g.executable);
    if (code < 0) {
        statusMessage_ = "No se pudo lanzar: " + g.executable;
        statusUntil_ = SDL_GetTicks() + 4000;
    }

    // 4. Al volver al menú: reabrir mando y reanudar música con fade in
    openController();
    initAudio();
    startMenuMusicWithFadeIn();
}

bool Menu::isSkipOrConfirmEvent(const SDL_Event& ev) const {
    if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
        SDL_Keycode sym = ev.key.keysym.sym;
        return (sym == SDLK_RETURN || sym == SDLK_KP_ENTER || sym == SDLK_SPACE || sym == SDLK_x);
    }
    if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
        Uint8 btn = ev.cbutton.button;
        return (btn == SDL_CONTROLLER_BUTTON_A || btn == SDL_CONTROLLER_BUTTON_START); // X u Options en DualShock 4
    }
    return false;
}

void Menu::handleEvent(const SDL_Event& ev) {
    if (ev.type == SDL_QUIT) {
        running_ = false;
        return;
    }

    // Dispositivos de mando conectados/desconectados en caliente
    if (ev.type == SDL_CONTROLLERDEVICEADDED) {
        openController();
    } else if (ev.type == SDL_CONTROLLERDEVICEREMOVED) {
        closeController();
        openController();
    }

    // Tecla M (o botón Back/Share/Touchpad) para silenciar/activar la música en cualquier momento
    if (ev.type == SDL_KEYDOWN && !ev.key.repeat && ev.key.keysym.sym == SDLK_m) {
        toggleMute();
        return;
    }
    if (ev.type == SDL_CONTROLLERBUTTONDOWN && ev.cbutton.button == SDL_CONTROLLER_BUTTON_BACK) {
        toggleMute();
        return;
    }

    // Tecla F11 para alternar pantalla completa y modo ventana manteniendo resolución lógica
    if (ev.type == SDL_KEYDOWN && !ev.key.repeat && ev.key.keysym.sym == SDLK_F11) {
        SDL_Window* win = SDL_RenderGetWindow(renderer_);
        if (win) {
            Uint32 flags = SDL_GetWindowFlags(win);
            bool isFull = (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
            SDL_SetWindowFullscreen(win, isFull ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
            SDL_RenderSetLogicalSize(renderer_, LOGICAL_WIDTH, LOGICAL_HEIGHT);
        }
        return;
    }

    // Manejo de eventos según el estado actual de la máquina de estados
    switch (state_) {
        case MenuState::SPLASH_ITF:
        case MenuState::SPLASH_LOGO:
            // Si el jugador pulsa X / Enter / Espacio tras el margen de 0.3 s, se salta el splash actual
            if (slideTimer_ >= SPLASH_SKIP_MARGIN && isSkipOrConfirmEvent(ev)) {
                advanceSponsorSlide();
            }
            break;

        case MenuState::INTRO_EPICA:
            // Si el jugador pulsa X / Enter / Espacio, salta directo a "Pulsa [X]" ya visible
            if (introTimer_ >= INTRO_SKIP_MARGIN && isSkipOrConfirmEvent(ev)) {
                skipToPulsaX();
            }
            break;

        case MenuState::PULSA_X:
            if (isSkipOrConfirmEvent(ev)) {
                state_ = MenuState::MENU;
            } else if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) {
                running_ = false;
            }
            break;

        case MenuState::MENU:
            switch (ev.type) {
                case SDL_KEYDOWN:
                    switch (ev.key.keysym.sym) {
                        case SDLK_UP:     moveUp(); break;
                        case SDLK_DOWN:   moveDown(); break;
                        case SDLK_RETURN:
                        case SDLK_SPACE:
                        case SDLK_x:      launchSelected(); break;
                        case SDLK_ESCAPE: running_ = false; break;
                        default: break;
                    }
                    break;

                case SDL_CONTROLLERBUTTONDOWN:
                    switch (ev.cbutton.button) {
                        case SDL_CONTROLLER_BUTTON_DPAD_UP:   moveUp(); break;
                        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: moveDown(); break;
                        case SDL_CONTROLLER_BUTTON_A:         launchSelected(); break; // X en PS4
                        case SDL_CONTROLLER_BUTTON_START:     launchSelected(); break;
                        case SDL_CONTROLLER_BUTTON_B:         running_ = false; break; // Círculo en PS4
                        default: break;
                    }
                    break;

                case SDL_CONTROLLERAXISMOTION:
                    if (ev.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                        const int v = ev.caxis.value;
                        const bool wasNeutral = (lastAxisY_ >= -8000 && lastAxisY_ <= 8000);
                        if (wasNeutral) {
                            if (v < -8000) moveUp();
                            else if (v > 8000) moveDown();
                        }
                        lastAxisY_ = v;
                    }
                    break;

                default:
                    break;
            }
            break;
    }
}

// ============================================================================
// BUCLE PRINCIPAL CON DELTA TIME (60 FPS ESTABLES, SIN SLEEPS BLOQUEANTES)
// ============================================================================

int Menu::run() {
    Uint64 perfFreq = SDL_GetPerformanceFrequency();
    Uint64 lastCounter = SDL_GetPerformanceCounter();

    while (running_) {
        Uint64 nowCounter = SDL_GetPerformanceCounter();
        float dt = static_cast<float>(nowCounter - lastCounter) / static_cast<float>(perfFreq);
        lastCounter = nowCounter;

        // Limitar dt ante pausas del sistema operativo o retorno de un minijuego
        if (dt > 0.1f) {
            dt = 1.0f / 60.0f;
        }

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            handleEvent(ev);
        }

        update(dt);
        render();

        // El renderer está creado con SDL_RENDERER_PRESENTVSYNC (60 FPS).
        // Si en algún driver el VSync no actúa y el frame tardó menos de 16.6ms,
        // cedemos brevemente la CPU sin bloquear la respuesta de eventos.
        Uint64 frameEnd = SDL_GetPerformanceCounter();
        float frameElapsedMs = static_cast<float>(frameEnd - nowCounter) * 1000.0f / static_cast<float>(perfFreq);
        if (frameElapsedMs < 16.0f) {
            SDL_Delay(static_cast<Uint32>(16.0f - frameElapsedMs));
        }
    }
    return 0;
}
