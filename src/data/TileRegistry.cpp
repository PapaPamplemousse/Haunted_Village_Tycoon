/**
 * @file TileRegistry.cpp
 * @brief Implementation of the TileRegistry.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "data/TileRegistry.hpp"

#include "data/STVParser.hpp"

#include <iostream>
#include <sstream>

Color TileRegistry::ParseColor(const std::string& value) {
    Color c = {255, 255, 255, 255};
    std::stringstream ss(value);
    std::string token;
    int i = 0;
    while (std::getline(ss, token, ',')) {
        if (i == 0)
            c.r = (unsigned char)std::stoi(token);
        else if (i == 1)
            c.g = (unsigned char)std::stoi(token);
        else if (i == 2)
            c.b = (unsigned char)std::stoi(token);
        else if (i == 3)
            c.a = (unsigned char)std::stoi(token);
        i++;
    }
    return c;
}

bool TileRegistry::LoadFromSTV(const std::string& filepath) {
    auto blocks = STVParser::Parse(filepath);
    if (blocks.empty())
        return false;

    for (const auto& block : blocks) {
        TileDef def;
        def.id = std::stoi(block.properties.at("id"));
        def.name = block.properties.at("name");
        def.walkable = (block.properties.at("walkable") == "true");

        if (block.properties.count("color")) {
            def.color = ParseColor(block.properties.at("color"));
        } else {
            def.color = MAGENTA; // Fallback missing color
        }

        m_tiles[def.id] = def;
        m_stringToIdMap[block.id] = def.id; // Map "SAND" -> 2
    }
    std::cout << "[INFO] Loaded " << m_tiles.size() << " tiles." << std::endl;
    return true;
}

const TileDef* TileRegistry::GetTileDef(int id) const {
    auto it = m_tiles.find(id);
    return it != m_tiles.end() ? &(it->second) : nullptr;
}

int TileRegistry::GetTileIdByString(const std::string& stringId) const {
    auto it = m_stringToIdMap.find(stringId);
    return it != m_stringToIdMap.end() ? it->second : -1;
}
