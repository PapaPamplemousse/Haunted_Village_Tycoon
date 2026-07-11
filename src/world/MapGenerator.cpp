#include "world/MapGenerator.hpp"

#include "core/Config.hpp"
#include "data/BiomeRegistry.hpp"
#include "data/EnvironmentRegistry.hpp"
#include "data/TileRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "world/PerlinNoise.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <raylib.h>
#include <vector>

namespace {

constexpr int WORLD_BORDER_SIZE = 2;

// Smaller value => larger, smoother climate zones.
constexpr float TEMPERATURE_NOISE_SCALE = 180.0f;
constexpr float HUMIDITY_NOISE_SCALE = 180.0f;
constexpr float CLIMATE_DETAIL_SCALE = 75.0f;

// Flora noise is smaller-scale than climate, but still smooth enough to create groves.
constexpr float VEGETATION_NOISE_SCALE = 18.0f;

bool IsInsideMap(int x, int y, int width, int height) {
    return x >= 0 && y >= 0 && x < width && y < height;
}

bool IsBorderTile(int x, int y, int width, int height) {
    return x < WORLD_BORDER_SIZE || y < WORLD_BORDER_SIZE || x >= width - WORLD_BORDER_SIZE || y >= height - WORLD_BORDER_SIZE;
}

float NormalizeNoise(float value) {
    return std::clamp((value + 1.0f) * 0.5f, 0.0f, 1.0f);
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
                     int voidTileId) {
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

            // Soft noisy border.
            const float normalizedDistance = distance / static_cast<float>(radius);

            if (normalizedDistance > 1.0f) {
                continue;
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
    SetRandomSeed(seed + 301);

    for (int y = WORLD_BORDER_SIZE; y < height - WORLD_BORDER_SIZE; ++y) {
        for (int x = WORLD_BORDER_SIZE; x < width - WORLD_BORDER_SIZE; ++x) {
            const BiomeDef* biome = biomeAt[static_cast<size_t>(y * width + x)];

            if (biome == nullptr || biome->flora.empty()) {
                continue;
            }

            const float nx = static_cast<float>(x) / VEGETATION_NOISE_SCALE;
            const float ny = static_cast<float>(y) / VEGETATION_NOISE_SCALE;

            const float vegetationScore = NormalizeNoise(vegetationNoise.GetNoise(nx, ny));

            Vector2 worldPos = {static_cast<float>(x) * Config::TILE_SIZE + Config::TILE_SIZE * 0.5f,
                                static_cast<float>(y) * Config::TILE_SIZE + Config::TILE_SIZE * 0.5f};

            for (const FloraSpawnDef& flora : biome->flora) {
                if (vegetationScore >= flora.noiseThreshold) {
                    envReg.SpawnEnvironment(em, flora.prefabId, worldPos);
                    break;
                }
            }
        }
    }
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

                ApplyPatchBiome(map, biomeAt, patch, px, py, radius, voidTileId);

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
