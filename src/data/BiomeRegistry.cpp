#include "data/BiomeRegistry.hpp"

#include "data/STVParser.hpp"
#include "data/TileRegistry.hpp"

#include <iostream>
#include <sstream>

bool BiomeRegistry::LoadFromSTV(const std::string& filepath, const TileRegistry& tileReg) {
    auto blocks = STVParser::Parse(filepath);
    if (blocks.empty())
        return false;

    for (const auto& block : blocks) {
        BiomeDef def;
        def.id = block.id;
        def.name = block.properties.at("name");

        std::string typeStr = block.properties.count("type") ? block.properties.at("type") : "climate";
        def.type = (typeStr == "patch") ? BiomeType::Patch : BiomeType::Climate;

        if (def.type == BiomeType::Patch) {
            def.minInstances = block.properties.count("min_instances") ? std::stoi(block.properties.at("min_instances")) : 1;
            def.maxInstances = block.properties.count("max_instances") ? std::stoi(block.properties.at("max_instances")) : 1;

            if (block.properties.count("radius_range")) {
                std::stringstream ssRad(block.properties.at("radius_range"));
                std::string rToken;
                std::getline(ssRad, rToken, ',');
                def.minRadius = std::stoi(rToken);
                std::getline(ssRad, rToken, ',');
                def.maxRadius = std::stoi(rToken);
            } else {
                def.minRadius = 1;
                def.maxRadius = 1;
            }
        }

        std::stringstream ssTemp(block.properties.at("temperature_range"));
        std::string tToken;
        std::getline(ssTemp, tToken, ',');
        def.minTemp = std::stof(tToken);
        std::getline(ssTemp, tToken, ',');
        def.maxTemp = std::stof(tToken);

        if (block.properties.count("flora")) {
            std::stringstream ss(block.properties.at("flora"));
            std::string token;
            while (std::getline(ss, token, ',')) {
                std::stringstream pairSS(token);
                std::string prefabId, thresholdStr;
                if (std::getline(pairSS, prefabId, ':') && std::getline(pairSS, thresholdStr)) {
                    prefabId.erase(0, prefabId.find_first_not_of(" \t")); // Trim
                    FloraSpawnDef fDef;
                    fDef.prefabId = prefabId;
                    fDef.noiseThreshold = std::stof(thresholdStr);
                    def.flora.push_back(fDef);
                }
            }
        }

        std::stringstream ssHum(block.properties.at("humidity_range"));
        std::string hToken;
        std::getline(ssHum, hToken, ',');
        def.minHum = std::stof(hToken);
        std::getline(ssHum, hToken, ',');
        def.maxHum = std::stof(hToken);

        def.baseTileId = tileReg.GetTileIdByString(block.properties.at("base_tile"));

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
    for (const auto& biome : m_climateBiomes) {
        if (temperature >= biome.minTemp && temperature <= biome.maxTemp && humidity >= biome.minHum && humidity <= biome.maxHum) {
            return &biome;
        }
    }
    return nullptr;
}
