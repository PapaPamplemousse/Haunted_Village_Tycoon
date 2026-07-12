/**
 * @file BiomeRegistry.cpp
 * @brief Implementation of the BiomeRegistry.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "data/BiomeRegistry.hpp"

#include "data/STVParser.hpp"
#include "data/TileRegistry.hpp"

#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string Trim(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");

    if (first == std::string::npos) {
        return "";
    }

    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

void ParseFloatRange(const std::string& value, float& outMin, float& outMax, float defaultMin, float defaultMax) {
    outMin = defaultMin;
    outMax = defaultMax;

    std::stringstream ss(value);
    std::string token;

    if (std::getline(ss, token, ',')) {
        outMin = std::stof(Trim(token));
    }

    if (std::getline(ss, token, ',')) {
        outMax = std::stof(Trim(token));
    }
}

void ParseIntRange(const std::string& value, int& outMin, int& outMax, int defaultMin, int defaultMax) {
    outMin = defaultMin;
    outMax = defaultMax;

    std::stringstream ss(value);
    std::string token;

    if (std::getline(ss, token, ',')) {
        outMin = std::stoi(Trim(token));
    }

    if (std::getline(ss, token, ',')) {
        outMax = std::stoi(Trim(token));
    }
}

std::vector<FloraSpawnDef> ParseFlora(const std::string& value) {
    std::vector<FloraSpawnDef> flora;

    std::stringstream ss(value);
    std::string token;

    while (std::getline(ss, token, ',')) {
        std::stringstream pairSS(token);

        std::string prefabId;
        std::string threshold;

        if (std::getline(pairSS, prefabId, ':') && std::getline(pairSS, threshold)) {
            FloraSpawnDef def;
            def.prefabId = Trim(prefabId);
            def.noiseThreshold = std::stof(Trim(threshold));

            if (!def.prefabId.empty()) {
                flora.push_back(def);
            }
        }
    }

    return flora;
}

} // namespace

bool BiomeRegistry::LoadFromSTV(const std::string& filepath, const TileRegistry& tileReg) {
    auto blocks = STVParser::Parse(filepath);

    if (blocks.empty()) {
        return false;
    }

    m_climateBiomes.clear();
    m_patchBiomes.clear();

    for (const auto& block : blocks) {
        BiomeDef def;
        def.id = block.id;

        if (block.properties.count("name")) {
            def.name = block.properties.at("name");
        } else {
            def.name = block.id;
        }

        std::string typeStr = "climate";

        if (block.properties.count("type")) {
            typeStr = Trim(block.properties.at("type"));
        }

        def.type = typeStr == "patch" ? BiomeType::Patch : BiomeType::Climate;

        if (block.properties.count("temperature_range")) {
            ParseFloatRange(block.properties.at("temperature_range"), def.minTemp, def.maxTemp, 0.0f, 50.0f);
        } else {
            def.minTemp = 0.0f;
            def.maxTemp = 50.0f;
        }

        if (block.properties.count("humidity_range")) {
            ParseFloatRange(block.properties.at("humidity_range"), def.minHum, def.maxHum, 0.0f, 100.0f);
        } else {
            def.minHum = 0.0f;
            def.maxHum = 100.0f;
        }

        if (block.properties.count("base_tile")) {
            def.base_tile = Trim(block.properties.at("base_tile"));
            def.baseTileId = tileReg.GetTileIdByString(def.base_tile);
        } else {
            def.base_tile = "";
            def.baseTileId = -1;
        }

        if (def.type == BiomeType::Patch) {
            if (block.properties.count("min_instances")) {
                def.minInstances = std::stoi(Trim(block.properties.at("min_instances")));
            } else {
                def.minInstances = 1;
            }

            if (block.properties.count("max_instances")) {
                def.maxInstances = std::stoi(Trim(block.properties.at("max_instances")));
            } else {
                def.maxInstances = def.minInstances;
            }

            if (block.properties.count("radius_range")) {
                ParseIntRange(block.properties.at("radius_range"), def.minRadius, def.maxRadius, 1, 1);
            } else {
                def.minRadius = 1;
                def.maxRadius = 1;
            }
        }

        if (block.properties.count("flora")) {
            def.flora = ParseFlora(block.properties.at("flora"));
        }

        if (def.type == BiomeType::Climate) {
            m_climateBiomes.push_back(def);
        } else {
            m_patchBiomes.push_back(def);
        }
    }

    std::cout << "[INFO] Loaded " << m_climateBiomes.size() << " Climate Biomes and " << m_patchBiomes.size() << " Patch Biomes."
              << std::endl;

    return true;
}

const BiomeDef* BiomeRegistry::GetClimateBiome(float temperature, float humidity) const {
    for (const BiomeDef& biome : m_climateBiomes) {
        if (temperature >= biome.minTemp && temperature <= biome.maxTemp && humidity >= biome.minHum && humidity <= biome.maxHum) {
            return &biome;
        }
    }

    return nullptr;
}
