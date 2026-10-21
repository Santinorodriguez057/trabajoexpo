#include "config.h"
#include <fstream>
#include <sstream>
#include <iostream>

static std::string resolveFile(const std::string& name) {
    if (FILE* f = std::fopen(name.c_str(), "rb")) {
        std::fclose(f);
        return name;
    }
    std::string p1 = "../" + name;
    if (FILE* f = std::fopen(p1.c_str(), "rb")) {
        std::fclose(f);
        return p1;
    }
    std::string p2 = "../../" + name;
    if (FILE* f = std::fopen(p2.c_str(), "rb")) {
        std::fclose(f);
        return p2;
    }
    return name;
}

void GameConfig::load(const std::string& filename) {
    // 1. Intentar cargar primero desde arqueria_settings.dat si existe
    std::string datPath = resolveFile("arqueria_settings.dat");
    std::ifstream datFile(datPath);
    if (datFile.is_open()) {
        float sx, sy, sm, dz;
        int inv, rum;
        if (datFile >> sx >> sy >> sm >> dz >> inv >> rum) {
            sensitivityX = sx;
            sensitivityY = sy;
            smoothing = sm;
            deadzone = dz;
            invertY = (inv != 0);
            rumbleEnabled = (rum != 0);
        }
    }

    // 2. Cargar configuracion por pares clave=valor si existe
    std::string cfgPath = resolveFile(filename);
    std::ifstream file(cfgPath);
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
    // Guardar en tiro_con_arco.cfg
    std::ofstream file(filename);
    if (file.is_open()) {
        file << "# Configuracion de Tiro con Arco\n";
        file << "sensitivityX=" << sensitivityX << "\n";
        file << "sensitivityY=" << sensitivityY << "\n";
        file << "smoothing=" << smoothing << "\n";
        file << "deadzone=" << deadzone << "\n";
        file << "invertY=" << (invertY ? "1" : "0") << "\n";
        file << "rumbleEnabled=" << (rumbleEnabled ? "1" : "0") << "\n";
        file << "highRecord=" << highRecord << "\n";
    }

    // Y sincronizar con arqueria_settings.dat
    std::ofstream datFile("arqueria_settings.dat");
    if (datFile.is_open()) {
        datFile << sensitivityX << " " << sensitivityY << " " << smoothing << " "
                << deadzone << " " << (invertY ? 1 : 0) << " " << (rumbleEnabled ? 1 : 0) << "\n";
    }
}
