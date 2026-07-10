#pragma once
#include <raylib.h>
#include <string>
#include <unordered_map>

/**
 * @struct TileDef
 * @brief Blueprint of a map tile loaded from tiles.stv
 */
struct TileDef {
    int id;
    std::string name;
    bool walkable;
    Color color;
};

/**
 * @class TileRegistry
 * @brief Parses and stores all tile definitions.
 */
class TileRegistry {
public:
    TileRegistry() = default;

    bool LoadFromSTV(const std::string& filepath);

    const TileDef* GetTileDef(int id) const;
    int GetTileIdByString(const std::string& stringId) const;

    const std::unordered_map<int, TileDef>& GetAllTiles() const {
        return m_tiles;
    }

private:
    std::unordered_map<int, TileDef> m_tiles;
    std::unordered_map<std::string, int> m_stringToIdMap;

    Color ParseColor(const std::string& value);
};
