#include "config.h"
#include <fstream>
#include <sstream>
#include <iostream>

void GameConfig::load(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string key;
        if (std::getline(iss, key, '=')) {
            std::string val;
            if (std::getline(iss, val)) {
                try {
                    if (key == "sensitivityX") sensitivityX = std::stof(val);
                    else if (key == "sensitivityY") sensitivityY = std::stof(val);
                    else if (key == "smoothing") smoothing = std::stof(val);
                    else if (key == "deadzone") deadzone = std::stof(val);
                    else if (key == "invertY") invertY = (val == "1" || val == "true");
                    else if (key == "rumbleEnabled") rumbleEnabled = (val == "1" || val == "true");
                    else if (key == "highRecord") highRecord = std::stoi(val);
                } catch (...) {}
            }
        }
    }
}

void GameConfig::save(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) return;

    file << "# Configuracion de Tiro con Arco\n";
    file << "sensitivityX=" << sensitivityX << "\n";
    file << "sensitivityY=" << sensitivityY << "\n";
    file << "smoothing=" << smoothing << "\n";
    file << "deadzone=" << deadzone << "\n";
    file << "invertY=" << (invertY ? "1" : "0") << "\n";
    file << "rumbleEnabled=" << (rumbleEnabled ? "1" : "0") << "\n";
    file << "highRecord=" << highRecord << "\n";
}
