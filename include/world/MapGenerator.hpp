/**
 * @file MapGenerator.hpp
 * @brief Handles procedural generation of the terrain, biomes, and flora.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "world/WorldMap.hpp"

/* Forward declaration */
class TileRegistry;
class BiomeRegistry;
class EnvironmentRegistry;
class EntityManager;

/**
 * @class MapGenerator
 * @brief Responsible for procedural terrain generation.
 */
class MapGenerator {
public:
    /**
     * @brief Generates a finite rectangular world using data-driven climate and patch biomes.
     */
    static void GenerateWorld(WorldMap& map, EntityManager& em, const TileRegistry& tileReg, const BiomeRegistry& biomeReg,
                              const EnvironmentRegistry& envReg, unsigned int seed);

    /**
     * @brief Legacy entry point kept for compatibility.
     * Internally calls GenerateWorld().
     */
    static void GenerateIsland(WorldMap& map, EntityManager& em, const TileRegistry& tileReg, const BiomeRegistry& biomeReg,
                               const EnvironmentRegistry& envReg, unsigned int seed);
};
