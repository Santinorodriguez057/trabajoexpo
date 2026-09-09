#pragma once

#include <string>
#include <vector>

// Una entrada del registro: un juego que el menu puede lanzar.
struct GameEntry {
    std::string id;          // identificador unico (sin espacios)
    std::string name;        // nombre visible en el menu
    std::string description; // descripcion corta
    std::string executable;  // ruta del ejecutable (resuelta para la plataforma actual)
};

// Carga el archivo games.registry (formato TSV, ver README).
class GameRegistry {
public:
    // Devuelve false si no se pudo abrir el archivo.
    bool load(const std::string& path);

    const std::vector<GameEntry>& games() const { return games_; }

private:
    std::vector<GameEntry> games_;
};