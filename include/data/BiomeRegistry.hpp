#pragma once
#include <string>
#include <vector>

class TileRegistry; // Forward declaration

/**
 * @enum BiomeType
 * @brief Represents the type of biome, either Climate or Patch.
 */
enum class BiomeType { Climate, Patch };

/**
 * @struct BiomeDef
 * @brief Blueprint of a biome, defining spawn conditions.
 */
struct BiomeDef {
    std::string id;
    std::string name;
    BiomeType type;

    // Constraints for Patch biomes
    int minInstances = 0;
    int maxInstances = 0;
    int minRadius = 1;
    int maxRadius = 1;

    // Climate requirements
    float minTemp, maxTemp;
    float minHum, maxHum;
    int baseTileId;
};

/**
 * @class BiomeRegistry
 * @brief Parses and stores all biome definitions.
 */
class BiomeRegistry {
public:
    BiomeRegistry() = default;

    bool LoadFromSTV(const std::string& filepath, const TileRegistry& tileReg);

    const BiomeDef* GetClimateBiome(float temperature, float humidity) const;
    const std::vector<BiomeDef>& GetPatchBiomes() const {
        return m_patchBiomes;
    }

private:
    std::vector<BiomeDef> m_climateBiomes;
    std::vector<BiomeDef> m_patchBiomes;
};
