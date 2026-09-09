#include "game_registry.h"

#include <fstream>
#include <sstream>

#if defined(_WIN32)
static const char* kPlatform = "windows";
#else
static const char* kPlatform = "linux";
#endif

static std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

bool GameRegistry::load(const std::string& path) {
    games_.clear();

    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        // Formato: id<TAB>nombre<TAB>descripcion<TAB>ejecutable_linux<TAB>ejecutable_windows
        std::istringstream ss(line);
        std::string id, name, desc, linuxPath, windowsPath;
        std::getline(ss, id, '\t');
        std::getline(ss, name, '\t');
        std::getline(ss, desc, '\t');
        std::getline(ss, linuxPath, '\t');
        std::getline(ss, windowsPath, '\t');

        id = trim(id);
        name = trim(name);
        if (id.empty() || name.empty()) continue;

        GameEntry e;
        e.id = id;
        e.name = name;
        e.description = trim(desc);
        e.executable = trim(std::string(kPlatform) == "windows" ? windowsPath : linuxPath);
        games_.push_back(e);
    }
    return true;
}