#pragma once

#include <string>

struct GameConfig {
    float sensitivityX = 1.6f;
    float sensitivityY = 1.4f;
    float smoothing = 0.35f;      // 0.0 (directo) a 0.9 (suavizado alto)
    float deadzone = 0.015f;      // deadzone optimizada para micro-apuntado suave
    bool invertY = false;
    bool rumbleEnabled = true;
    int highRecord = 0;

    void load(const std::string& filename = "tiro_con_arco.cfg");
    void save(const std::string& filename = "tiro_con_arco.cfg") const;
};
