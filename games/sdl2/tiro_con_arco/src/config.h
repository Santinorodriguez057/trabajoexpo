#pragma once

#include <string>

struct GameConfig {
    float sensitivityX = 1.0f;
    float sensitivityY = 1.0f;
    float smoothing = 0.5f;       // 0.0 (direct) to 0.9 (heavy smoothing)
    float deadzone = 0.04f;       // rad/s gyro deadzone
    bool invertY = false;
    bool rumbleEnabled = true;
    int highRecord = 0;

    void load(const std::string& filename = "tiro_con_arco.cfg");
    void save(const std::string& filename = "tiro_con_arco.cfg") const;
};
