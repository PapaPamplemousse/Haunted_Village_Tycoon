#pragma once
#include "world/WorldMap.hpp"

/* Forward declaration */
class TileRegistry;
class BiomeRegistry;

/**
 * @class MapGenerator
 * @brief Responsible for procedural terrain generation.
 */
class MapGenerator {
public:
    /**
     * @brief Generates an island using Perlin noise and a radial gradient.
     * @param map The WorldMap instance to populate.
     * @param seed The random seed for generation.
     */
    static void GenerateIsland(WorldMap& map, const TileRegistry& tileReg, const BiomeRegistry& biomeReg, unsigned int seed);
};
