/**
 * @file MapGenerator.cpp
 * @brief Implementation of the procedural map generation.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "world/MapGenerator.hpp"

#include "core/Config.hpp"
#include "data/BiomeRegistry.hpp"
#include "data/EnvironmentRegistry.hpp"
#include "data/TileRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "world/PerlinNoise.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <raylib.h>
#include <string>
#include <vector>

namespace {

constexpr int WORLD_BORDER_SIZE = 2;

// Smaller value => larger, smoother climate zones.
constexpr float TEMPERATURE_NOISE_SCALE = 180.0f;
constexpr float HUMIDITY_NOISE_SCALE = 180.0f;
constexpr float CLIMATE_DETAIL_SCALE = 75.0f;

// Flora generation V2.
// Higher scale => larger vegetation patterns.
// Actual spawning is then filtered by blue-noise spacing.
constexpr float VEGETATION_NOISE_SCALE = 34.0f;
constexpr float VEGETATION_DETAIL_SCALE = 11.0f;

constexpr int TREE_MIN_DISTANCE_TILES = 1;
constexpr int BUSH_MIN_DISTANCE_TILES = 2;

constexpr int BUSH_TREE_EXCLUSION_RADIUS_TILES = 1;
constexpr int BUSH_TREE_EDGE_RADIUS_TILES = 5;

bool IsInsideMap(int x, int y, int width, int height) {
    return x >= 0 && y >= 0 && x < width && y < height;
}

bool IsBorderTile(int x, int y, int width, int height) {
    return x < WORLD_BORDER_SIZE || y < WORLD_BORDER_SIZE || x >= width - WORLD_BORDER_SIZE || y >= height - WORLD_BORDER_SIZE;
}

float NormalizeNoise(float value) {
    return std::clamp((value + 1.0f) * 0.5f, 0.0f, 1.0f);
}

struct TilePoint {
    int x = 0;
    int y = 0;
};

uint32_t HashU32(int x, int y, unsigned int seed) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u + static_cast<uint32_t>(seed) * 2246822519u;

    h = (h ^ (h >> 13u)) * 1274126177u;
    return h ^ (h >> 16u);
}

float Hash01(int x, int y, unsigned int seed) {
    return static_cast<float>(HashU32(x, y, seed) & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

Vector2 TileToWorldCenter(int tileX, int tileY) {
    return {static_cast<float>(tileX) * Config::TILE_SIZE + Config::TILE_SIZE * 0.5f,
            static_cast<float>(tileY) * Config::TILE_SIZE + Config::TILE_SIZE * 0.5f};
}

bool IsFarEnoughFromPoints(const std::vector<TilePoint>& points, int x, int y, int minDistanceTiles) {
    const int minDistSq = minDistanceTiles * minDistanceTiles;

    for (const TilePoint& point : points) {
        const int dx = point.x - x;
        const int dy = point.y - y;

        if (dx * dx + dy * dy < minDistSq) {
            return false;
        }
    }

    return true;
}

int CountNearbyPoints(const std::vector<TilePoint>& points, int x, int y, int radiusTiles) {
    const int radiusSq = radiusTiles * radiusTiles;
    int count = 0;

    for (const TilePoint& point : points) {
        const int dx = point.x - x;
        const int dy = point.y - y;

        if (dx * dx + dy * dy <= radiusSq) {
            count++;
        }
    }

    return count;
}

bool HasFloraPrefabContaining(const BiomeDef& biome, const std::string& token, std::string& outPrefabId, float& outThreshold) {
    for (const FloraSpawnDef& flora : biome.flora) {
        if (flora.prefabId.find(token) == std::string::npos) {
            continue;
        }

        outPrefabId = flora.prefabId;
        outThreshold = flora.noiseThreshold;
        return true;
    }

    return false;
}

bool IsOccupied(const std::vector<bool>& occupied, int width, int x, int y) {
    return occupied[static_cast<size_t>(y * width + x)];
}

void MarkOccupied(std::vector<bool>& occupied, int width, int x, int y) {
    occupied[static_cast<size_t>(y * width + x)] = true;
}

std::vector<bool> BuildOccupiedTileMap(const EntityManager& em, int width, int height) {
    std::vector<bool> occupied(static_cast<size_t>(width * height), false);

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!em.active[entity] || !em.hasTransform[entity]) {
            continue;
        }

        const int x = static_cast<int>(std::floor(em.transforms[entity].position.x / Config::TILE_SIZE));
        const int y = static_cast<int>(std::floor(em.transforms[entity].position.y / Config::TILE_SIZE));

        if (!IsInsideMap(x, y, width, height)) {
            continue;
        }

        occupied[static_cast<size_t>(y * width + x)] = true;
    }

    return occupied;
}

float DistanceToRange(float value, float minValue, float maxValue) {
    if (value < minValue) {
        return minValue - value;
    }

    if (value > maxValue) {
        return value - maxValue;
    }

    return 0.0f;
}

const BiomeDef* FindBestClimateBiome(float temperature, float humidity, const BiomeRegistry& biomeReg) {
    const BiomeDef* exactMatch = biomeReg.GetClimateBiome(temperature, humidity);

    if (exactMatch != nullptr) {
        return exactMatch;
    }

    // Fallback: choose the nearest climate biome.
    // This avoids unexpected default tiles if climate ranges do not cover 100% of the map.
    const BiomeDef* bestBiome = nullptr;
    float bestScore = std::numeric_limits<float>::infinity();

    for (const BiomeDef& biome : biomeReg.GetClimateBiomes()) {
        const float tempDistance = DistanceToRange(temperature, biome.minTemp, biome.maxTemp);

        const float humidityDistance = DistanceToRange(humidity, biome.minHum, biome.maxHum);

        const float score = tempDistance * tempDistance + humidityDistance * humidityDistance;

        if (score < bestScore) {
            bestScore = score;
            bestBiome = &biome;
        }
    }

    return bestBiome;
}

float ComputeTemperature(int x, int y, int width, int height, const PerlinNoise& temperatureNoise) {
    const float nx = static_cast<float>(x) / TEMPERATURE_NOISE_SCALE;
    const float ny = static_cast<float>(y) / TEMPERATURE_NOISE_SCALE;

    const float detailX = static_cast<float>(x) / CLIMATE_DETAIL_SCALE;
    const float detailY = static_cast<float>(y) / CLIMATE_DETAIL_SCALE;

    const float large = NormalizeNoise(temperatureNoise.GetNoise(nx, ny));
    const float detail = NormalizeNoise(temperatureNoise.GetNoise(detailX + 51.7f, detailY + 12.3f));

    // Slight north/south gradient to reduce pure noise randomness.
    const float latitude = height > 1 ? static_cast<float>(y) / static_cast<float>(height - 1) : 0.5f;

    const float latitudeWarmth = 1.0f - std::abs(latitude - 0.5f) * 0.35f;

    const float temperature01 = std::clamp(large * 0.65f + detail * 0.20f + latitudeWarmth * 0.15f, 0.0f, 1.0f);

    return temperature01 * 50.0f;
}

float ComputeHumidity(int x, int y, const PerlinNoise& humidityNoise) {
    const float nx = static_cast<float>(x) / HUMIDITY_NOISE_SCALE;
    const float ny = static_cast<float>(y) / HUMIDITY_NOISE_SCALE;

    const float detailX = static_cast<float>(x) / CLIMATE_DETAIL_SCALE;
    const float detailY = static_cast<float>(y) / CLIMATE_DETAIL_SCALE;

    const float large = NormalizeNoise(humidityNoise.GetNoise(nx + 133.0f, ny + 91.0f));
    const float detail = NormalizeNoise(humidityNoise.GetNoise(detailX + 7.0f, detailY + 19.0f));

    return std::clamp(large * 0.75f + detail * 0.25f, 0.0f, 1.0f) * 100.0f;
}

void ApplyPatchBiome(WorldMap& map, std::vector<const BiomeDef*>& biomeAt, const BiomeDef& patch, int centerX, int centerY, int radius,
                     int voidTileId, unsigned int seed) {
    const int width = map.GetWidth();
    const int height = map.GetHeight();

    for (int y = centerY - radius; y <= centerY + radius; ++y) {
        for (int x = centerX - radius; x <= centerX + radius; ++x) {
            if (!IsInsideMap(x, y, width, height)) {
                continue;
            }

            if (IsBorderTile(x, y, width, height)) {
                continue;
            }

            const int currentTileId = map.GetTile(x, y);

            if (currentTileId == voidTileId) {
                continue;
            }

            const float dx = static_cast<float>(x - centerX);
            const float dy = static_cast<float>(y - centerY);
            const float distance = std::sqrt(dx * dx + dy * dy);

            const float normalizedDistance = distance / static_cast<float>(radius);

            if (normalizedDistance > 1.0f) {
                continue;
            }

            // Organic noisy border:
            // core is always kept, edges are probabilistic.
            if (normalizedDistance > 0.55f) {
                const float edgeStrength = 1.0f - normalizedDistance;
                const float noise = Hash01(x, y, seed);

                if (noise > edgeStrength * 1.8f) {
                    continue;
                }
            }

            map.SetTile(x, y, patch.baseTileId);
            biomeAt[static_cast<size_t>(y * width + x)] = &patch;
        }
    }
}

void SpawnFlora(WorldMap& map, EntityManager& em, const EnvironmentRegistry& envReg, const std::vector<const BiomeDef*>& biomeAt,
                unsigned int seed) {
    const int width = map.GetWidth();
    const int height = map.GetHeight();

    PerlinNoise vegetationNoise(seed + 300);
    PerlinNoise detailNoise(seed + 301);
    PerlinNoise warpNoise(seed + 302);

    std::vector<TilePoint> treePoints;
    std::vector<TilePoint> bushPoints;
    std::vector<bool> occupied = BuildOccupiedTileMap(em, width, height);

    // =========================================================
    // PASS 1: Trees
    // =========================================================
    for (int y = WORLD_BORDER_SIZE; y < height - WORLD_BORDER_SIZE; ++y) {
        for (int x = WORLD_BORDER_SIZE; x < width - WORLD_BORDER_SIZE; ++x) {
            const BiomeDef* biome = biomeAt[static_cast<size_t>(y * width + x)];

            if (biome == nullptr || biome->flora.empty()) {
                continue;
            }

            std::string treePrefab;
            float treeThreshold = 1.0f;

            if (!HasFloraPrefabContaining(*biome, "TREE", treePrefab, treeThreshold)) {
                continue;
            }

            if (IsOccupied(occupied, width, x, y)) {
                continue;
            }

            // Domain warp to avoid obvious round/rectangular blobs.
            const float warpX =
                NormalizeNoise(warpNoise.GetNoise(static_cast<float>(x) / 90.0f + 12.4f, static_cast<float>(y) / 90.0f - 3.7f)) - 0.5f;

            const float warpY =
                NormalizeNoise(warpNoise.GetNoise(static_cast<float>(x) / 90.0f - 31.0f, static_cast<float>(y) / 90.0f + 44.2f)) - 0.5f;

            const float warpedX = static_cast<float>(x) + warpX * 16.0f;
            const float warpedY = static_cast<float>(y) + warpY * 16.0f;

            const float vegetationScore =
                NormalizeNoise(vegetationNoise.GetNoise(warpedX / VEGETATION_NOISE_SCALE, warpedY / VEGETATION_NOISE_SCALE));

            if (vegetationScore < treeThreshold) {
                continue;
            }

            const float detail = NormalizeNoise(
                detailNoise.GetNoise(static_cast<float>(x) / VEGETATION_DETAIL_SCALE, static_cast<float>(y) / VEGETATION_DETAIL_SCALE));

            // The biome threshold gives the eligible area, but probability keeps it sparse.
            float spawnProbability = 0.04f;
            spawnProbability += (vegetationScore - treeThreshold) * 0.55f;
            spawnProbability += detail * 0.04f;
            spawnProbability = std::clamp(spawnProbability, 0.02f, 0.24f);

            if (Hash01(x, y, seed + 700) > spawnProbability) {
                continue;
            }

            // Blue-noise spacing: prevents dense blocks of trees.
            if (!IsFarEnoughFromPoints(treePoints, x, y, TREE_MIN_DISTANCE_TILES)) {
                continue;
            }

            envReg.SpawnEnvironment(em, treePrefab, TileToWorldCenter(x, y));
            treePoints.push_back({x, y});
            MarkOccupied(occupied, width, x, y);
        }
    }

    // =========================================================
    // PASS 2: Bushes
    // =========================================================
    for (int y = WORLD_BORDER_SIZE; y < height - WORLD_BORDER_SIZE; ++y) {
        for (int x = WORLD_BORDER_SIZE; x < width - WORLD_BORDER_SIZE; ++x) {
            const BiomeDef* biome = biomeAt[static_cast<size_t>(y * width + x)];

            if (biome == nullptr || biome->flora.empty()) {
                continue;
            }

            std::string bushPrefab;
            float bushThreshold = 1.0f;

            if (!HasFloraPrefabContaining(*biome, "BUSH", bushPrefab, bushThreshold)) {
                continue;
            }

            if (IsOccupied(occupied, width, x, y)) {
                continue;
            }

            const int veryNearbyTrees = CountNearbyPoints(treePoints, x, y, BUSH_TREE_EXCLUSION_RADIUS_TILES);

            // Do not bury bushes directly inside tree clusters.
            if (veryNearbyTrees > 0) {
                continue;
            }

            const int nearbyTrees = CountNearbyPoints(treePoints, x, y, BUSH_TREE_EDGE_RADIUS_TILES);

            float edgeBonus = 0.0f;

            // Favor forest edges: some nearby trees, but not too many.
            if (nearbyTrees >= 1 && nearbyTrees <= 5) {
                edgeBonus = 0.12f;
            }

            const float bushPatch =
                NormalizeNoise(vegetationNoise.GetNoise(static_cast<float>(x) / 26.0f + 71.0f, static_cast<float>(y) / 26.0f - 19.0f));

            if (bushPatch < bushThreshold) {
                continue;
            }

            float spawnProbability = 0.025f;
            spawnProbability += edgeBonus;
            spawnProbability += (bushPatch - bushThreshold) * 0.30f;
            spawnProbability = std::clamp(spawnProbability, 0.015f, 0.20f);

            if (Hash01(x, y, seed + 900) > spawnProbability) {
                continue;
            }

            // Bushes need stronger spacing than trees to avoid berry walls.
            if (!IsFarEnoughFromPoints(bushPoints, x, y, BUSH_MIN_DISTANCE_TILES)) {
                continue;
            }

            envReg.SpawnEnvironment(em, bushPrefab, TileToWorldCenter(x, y));
            bushPoints.push_back({x, y});
            MarkOccupied(occupied, width, x, y);
        }
    }

    std::cout << "[WORLDGEN] Flora V2 spawned " << treePoints.size() << " trees and " << bushPoints.size() << " bushes." << std::endl;
}

} // namespace

void MapGenerator::GenerateWorld(WorldMap& map, EntityManager& em, const TileRegistry& tileReg, const BiomeRegistry& biomeReg,
                                 const EnvironmentRegistry& envReg, unsigned int seed) {
    const int width = map.GetWidth();
    const int height = map.GetHeight();

    if (width <= 0 || height <= 0) {
        return;
    }

    const int voidTileId = tileReg.GetTileIdByString("VOID");
    const int fallbackTileId = tileReg.GetTileIdByString("SAND");

    PerlinNoise temperatureNoise(seed + 100);
    PerlinNoise humidityNoise(seed + 200);

    std::vector<float> temperatureMap(static_cast<size_t>(width * height), 0.0f);
    std::vector<float> humidityMap(static_cast<size_t>(width * height), 0.0f);
    std::vector<const BiomeDef*> biomeAt(static_cast<size_t>(width * height), nullptr);

    // ==========================================
    // PASS 1: FINITE RECTANGULAR CLIMATE MAP
    // ==========================================
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int index = y * width + x;

            if (IsBorderTile(x, y, width, height)) {
                map.SetTile(x, y, voidTileId >= 0 ? voidTileId : fallbackTileId);
                biomeAt[static_cast<size_t>(index)] = nullptr;
                continue;
            }

            const float temperature = ComputeTemperature(x, y, width, height, temperatureNoise);
            const float humidity = ComputeHumidity(x, y, humidityNoise);

            temperatureMap[static_cast<size_t>(index)] = temperature;
            humidityMap[static_cast<size_t>(index)] = humidity;

            const BiomeDef* biome = FindBestClimateBiome(temperature, humidity, biomeReg);

            if (biome != nullptr) {
                map.SetTile(x, y, biome->baseTileId);
                biomeAt[static_cast<size_t>(index)] = biome;
            } else {
                map.SetTile(x, y, fallbackTileId);
                biomeAt[static_cast<size_t>(index)] = nullptr;
            }
        }
    }

    // ==========================================
    // PASS 2: DATA-DRIVEN PATCH BIOMES
    // ==========================================
    SetRandomSeed(seed + 500);

    for (const BiomeDef& patch : biomeReg.GetPatchBiomes()) {
        if (patch.maxInstances <= 0) {
            continue;
        }

        const int minInstances = std::max(0, patch.minInstances);
        const int maxInstances = std::max(minInstances, patch.maxInstances);
        const int instanceCount = GetRandomValue(minInstances, maxInstances);

        for (int instance = 0; instance < instanceCount; ++instance) {
            bool placed = false;

            for (int attempt = 0; attempt < 200 && !placed; ++attempt) {
                const int px = GetRandomValue(WORLD_BORDER_SIZE, width - WORLD_BORDER_SIZE - 1);
                const int py = GetRandomValue(WORLD_BORDER_SIZE, height - WORLD_BORDER_SIZE - 1);

                const int index = py * width + px;

                const float temperature = temperatureMap[static_cast<size_t>(index)];
                const float humidity = humidityMap[static_cast<size_t>(index)];

                if (temperature < patch.minTemp || temperature > patch.maxTemp || humidity < patch.minHum || humidity > patch.maxHum) {
                    continue;
                }

                const int minRadius = std::max(1, patch.minRadius);
                const int maxRadius = std::max(minRadius, patch.maxRadius);
                const int radius = GetRandomValue(minRadius, maxRadius);

                ApplyPatchBiome(map, biomeAt, patch, px, py, radius, voidTileId,
                                seed + static_cast<unsigned int>(instance * 997 + px * 31 + py * 17));

                placed = true;
            }
        }
    }

    // ==========================================
    // PASS 3: DATA-DRIVEN FLORA
    // ==========================================
    SpawnFlora(map, em, envReg, biomeAt, seed);
}

void MapGenerator::GenerateIsland(WorldMap& map, EntityManager& em, const TileRegistry& tileReg, const BiomeRegistry& biomeReg,
                                  const EnvironmentRegistry& envReg, unsigned int seed) {
    GenerateWorld(map, em, tileReg, biomeReg, envReg, seed);
}
