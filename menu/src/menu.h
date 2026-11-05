#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>

#include <string>
#include <vector>
#include <unordered_map>

#include "game_launcher.h"
#include "game_registry.h"

// ============================================================================
// CONSTANTES CONFIGURABLES: SPLASH SCREENS, ANIMACIÓN ÉPICA Y AUDIO
// ============================================================================

// Resolución lógica base (independiente del tamaño real de ventana o monitor)
constexpr int LOGICAL_WIDTH  = 1920;
constexpr int LOGICAL_HEIGHT = 1080;

// Colores de fondo de cada Splash Screen
constexpr SDL_Color SPLASH1_BG_COLOR = {255, 255, 255, 255}; // Blanco sólido (#FFFFFF) para ITF
constexpr SDL_Color SPLASH2_BG_COLOR = {18, 20, 26, 255};    // Fondo oscuro elegante para Mi Logo (configurable)

// Escala de los logos respecto al ancho de la pantalla (~55 % de la ventana, sin deformar)
constexpr float SPLASH_LOGO_MAX_W_RATIO = 0.55f;
constexpr float SPLASH_LOGO_MAX_H_RATIO = 0.60f;

// Tiempos de cada Splash Screen (en segundos)
constexpr float SPLASH_FADE_IN_DURATION  = 0.6f; // Fade in 0.6 s
constexpr float SPLASH_HOLD_DURATION     = 2.0f; // Permanece 2.0 s
constexpr float SPLASH_FADE_OUT_DURATION = 0.6f; // Fade out 0.6 s
constexpr float SPLASH_SKIP_MARGIN       = 0.3f; // Margen inicial para evitar saltos accidentales

// Parámetros de la Animación Épica de Intro (~4.0 s en total)
constexpr float INTRO_PHASE1_END    = 1.0f; // (0.0-1.0 s) Pantalla oscura + línea de luz horizontal
constexpr float INTRO_PHASE2_END    = 2.0f; // (1.0-2.0 s) Wipe de barras neobrutalistas + zoom (1.15 -> 1.0)
constexpr float INTRO_PHASE3_END    = 3.0f; // (2.0-3.0 s) Golpe del título (3x -> 1x ease-out-back) + flash + shake
constexpr float INTRO_PHASE4_END    = 4.0f; // (3.0-4.0 s) Chispas/partículas + rayos radiales + shine sweep
constexpr float INTRO_SKIP_MARGIN   = 0.25f;

constexpr int   INTRO_WIPE_BARS         = 9;     // Cantidad de barras gruesas del wipe neobrutalista
constexpr float INTRO_BG_ZOOM_START     = 1.15f; // Escala inicial del zoom del fondo
constexpr float INTRO_TITLE_SCALE_START = 3.0f;  // Escala inicial del título antes del impacto
constexpr float INTRO_SHAKE_AMPLITUDE   = 8.0f;  // Amplitud del screen shake en píxeles (~8 px)
constexpr float INTRO_SHAKE_DURATION    = 0.3f;  // Duración del decaimiento del shake (0.3 s)
constexpr int   INTRO_FLASH_FRAMES      = 3;     // Duración del flash blanco al impacto (2-3 frames)
constexpr int   INTRO_MAX_PARTICLES     = 140;   // Cantidad de chispas en el impacto (máx. ~150)
constexpr int   INTRO_SUNBURST_RAYS     = 12;    // Cantidad de rayos radiales detrás del título

// Colores parametrizados de la Intro Neobrutalista
constexpr SDL_Color INTRO_DARK_BG_COLOR   = {10, 10, 14, 255};
constexpr SDL_Color INTRO_LIGHT_LINE_COL  = {250, 204, 21, 255}; // Dorado energético
constexpr SDL_Color INTRO_ACCENT_CYAN_COL = {56, 189, 248, 255}; // Cian neobrutalista
constexpr SDL_Color INTRO_ACCENT_PINK_COL = {244, 63, 94, 255};  // Rosa/Rojo impacto

// Configuración de Música y Audio del Menú
constexpr const char* MENU_MUSIC_PATH        = "assets/audio/menu.ogg";
constexpr const char* MENU_HIT_SFX_PATH      = "assets/audio/hit.wav";
constexpr float       MENU_MUSIC_VOLUME_PCT  = 0.70f; // Volumen por defecto 70 %
constexpr int         MENU_MUSIC_FADE_IN_MS  = 1500;  // Fade in de música de 1.5 s
constexpr int         MENU_MUSIC_FADE_OUT_MS = 350;   // Fade out rápido antes de lanzar un minijuego

// ============================================================================
// MÁQUINA DE ESTADOS Y ESTRUCTURAS DE DATOS
// ============================================================================

enum class MenuState {
    SPLASH_ITF,   // Pantalla 1: Sponsor ITF
    SPLASH_LOGO,  // Pantalla 2: Mi logo (y slides adicionales si se agregan a la lista)
    INTRO_EPICA,  // Animación épica por fases (0.0 s -> 4.0 s)
    PULSA_X,      // Pantalla de título interactiva con "Pulsa [X]" en bucle
    MENU          // Selector de proyectos / minijuegos
};

// Estructura para cada slide de Sponsor en la lista extensible
struct SponsorSlide {
    std::string name;                     // Nombre identificador (para logs)
    std::vector<std::string> imagePaths;  // Rutas candidatas (PNG/BMP)
    SDL_Color bgColor;                    // Color de fondo de este slide
    float maxWidthRatio;                  // Fracción máxima del ancho de pantalla
    float maxHeightRatio;                 // Fracción máxima del alto de pantalla
    SDL_Texture* texture = nullptr;       // Textura cargada
    int texW = 0;                         // Ancho original de la imagen
    int texH = 0;                         // Alto original de la imagen
    bool loaded = false;                  // Indica si se pudo cargar con éxito
};

// Partícula para la explosión de chispas del impacto del título
struct IntroParticle {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float size = 8.0f;
    float angle = 0.0f;
    float angularVel = 0.0f;
    float life = 0.0f;
    float maxLife = 1.0f;
    SDL_Color color = {250, 204, 21, 255};
    bool active = false;
};

// Menú principal con máquina de estados, Splash Screens, Intro Épica y Audio SDL_mixer.
class Menu {
public:
    Menu(SDL_Renderer* renderer, TTF_Font* font, const std::vector<GameEntry>& games);
    ~Menu();

    // Bucle principal a 60 FPS con delta time. Devuelve el código de salida del menú.
    int run();

private:
    // Gestión de mandos DualShock 4 / Gamepad
    void openController();
    void closeController();

    // Gestión de imágenes y slides de sponsors
    SDL_Surface* loadSurfaceSafe(const std::string& path);
    void initSponsorSlides();
    void freeSponsorSlides();
    void advanceSponsorSlide();

    // Gestión de audio con SDL_mixer
    void initAudio();
    void closeAudio();
    void startMenuMusicWithFadeIn();
    void triggerIntroHitAudio();
    void toggleMute();

    // Lógica de la animación épica
    void spawnImpactParticles(float centerX, float centerY);
    void updateImpactParticles(float dt);
    void skipToPulsaX();

    // Actualización y renderizado por estado (Máquina de estados)
    void update(float dt);
    void render();

    void updateSplash(float dt);
    void updateIntroEpica(float dt);
    void updatePulsaX(float dt);
    void updateMenu(float dt);

    void renderSplash();
    void renderIntroEpica();
    void renderPulsaX();
    void renderMenu();

    // Utilidades de dibujo neobrutalista
    void renderAnimatedGridBackground(float zoomScale, int offsetX = 0, int offsetY = 0);
    void renderFloatingConfetti(int offsetX = 0, int offsetY = 0);
    void renderTitleBanner(float scale, int offsetX, int offsetY, float shineProgress);
    void renderSunburstRays(int centerX, int centerY, float rotationRad, float alphaNorm);
    void renderWatermark(Uint8 alpha = 75);
    void renderMuteIndicator();
    void renderText(const std::string& text, int x, int y, SDL_Color color, float scale = 1.0f, bool center = false, Uint8 alpha = 255);

    // Eventos e interacción
    void handleEvent(const SDL_Event& ev);
    bool isSkipOrConfirmEvent(const SDL_Event& ev) const;
    void moveUp();
    void moveDown();
    void launchSelected();

    SDL_Renderer* renderer_;
    TTF_Font* font_;
    const std::vector<GameEntry>& games_;
    SDL_GameController* controller_ = nullptr;

    // Estado actual de la máquina de estados
    MenuState state_ = MenuState::SPLASH_ITF;
    float globalTime_ = 0.0f;

    // Lista extensible de slides de sponsors
    std::vector<SponsorSlide> slides_;
    size_t currentSlideIndex_ = 0;
    float slideTimer_ = 0.0f;

    // Estado de la Animación Épica de Intro
    float introTimer_ = 0.0f;
    bool hitTriggered_ = false;
    int flashFramesRemaining_ = 0;
    float shakeTimer_ = 0.0f;
    float pulsaXFadeAlpha_ = 0.0f;
    std::vector<IntroParticle> particles_;

    // Estado del selector del menú
    int selected_ = 0;
    bool running_ = true;
    int lastAxisY_ = 0;

    std::string statusMessage_;
    Uint32 statusUntil_ = 0;

    std::vector<float> hoverValues_;
    float scrollY_ = 0.0f;

    // Caché de texturas de texto y marca de agua
    std::unordered_map<std::string, SDL_Texture*> textCache_;
    SDL_Texture* watermarkTex_ = nullptr;

    // Audio SDL_mixer
    bool audioInitialized_ = false;
    bool musicStarted_ = false;
    bool isMuted_ = false;
    Mix_Music* menuMusic_ = nullptr;
    Mix_Chunk* hitSfx_ = nullptr;

    GameLauncher launcher_;
};
