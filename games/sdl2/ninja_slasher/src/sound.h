#pragma once

#include <SDL_mixer.h>

class SoundSystem {
public:
    SoundSystem();
    ~SoundSystem();

    bool init();
    void shutdown();

    // Control de música dinámica (Menú vs Partida)
    void startMenuBgm();
    void startGameBgm();
    void setBgmDuck(bool slowMoOrFreeze);

    // Sonidos de interfaz y flujo
    void playUiNav();
    void playUiConfirm();
    void playCountdownBeep(bool isGo);
    void playGameOver(bool victoryOrRecord);

    // Sonidos de katana e impactos
    void playWhoosh(int playerIndex, bool isFrenzy = false);
    void playSlice(int rating, bool isShuriken); // rating: 1=GRAZE, 2=GOOD, 3=PERFECT
    void playKillstreak(int playerIndex, int combo);
    void playMultiSlice(int count); // SLICE x2, x3, x4+
    void playMineExplosion();
    void playDropMiss();
    void playPowerup(int powerupKind); // 0=Freeze, 1=Dual, 2=Frenzy
    void playDangerHeartbeat();

private:
    bool ready_ = false;

    Mix_Chunk* bgmMenu_ = nullptr;
    Mix_Chunk* bgmGame_ = nullptr;
    int bgmChannel_ = 0; // Canal reservado 0 para BGM
    int currentBgmMode_ = 0; // 0=ninguno, 1=menu, 2=game

    Mix_Chunk* sfxUiNav_ = nullptr;
    Mix_Chunk* sfxUiConfirm_ = nullptr;
    Mix_Chunk* sfxCountBeep_ = nullptr;
    Mix_Chunk* sfxCountGo_ = nullptr;
    Mix_Chunk* sfxWhoosh_ = nullptr;
    Mix_Chunk* sfxWhooshFire_ = nullptr;
    Mix_Chunk* sfxGraze_ = nullptr;
    Mix_Chunk* sfxSliceGood_ = nullptr;
    Mix_Chunk* sfxSlicePerfect_ = nullptr;
    Mix_Chunk* sfxShurikenHit_ = nullptr;
    Mix_Chunk* sfxMultiSlice_[3] = {nullptr, nullptr, nullptr}; // x2, x3, x4+
    Mix_Chunk* sfxExplode_ = nullptr;
    Mix_Chunk* sfxDropMiss_ = nullptr;
    Mix_Chunk* sfxPowerFreeze_ = nullptr;
    Mix_Chunk* sfxPowerDual_ = nullptr;
    Mix_Chunk* sfxPowerFrenzy_ = nullptr;
    Mix_Chunk* sfxHeartbeat_ = nullptr;
    Mix_Chunk* sfxGameOverLose_ = nullptr;
    Mix_Chunk* sfxGameOverWin_ = nullptr;
    Mix_Chunk* sfxKillstreak_[8] = {nullptr};
};
