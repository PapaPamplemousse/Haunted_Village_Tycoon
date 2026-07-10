#include "world/MapGenerator.hpp"

#include "core/Config.hpp"
#include "data/BiomeRegistry.hpp"
#include "data/EnvironmentRegistry.hpp"
#include "data/TileRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "world/PerlinNoise.hpp"

#include <cmath>
#include <raylib.h> // Pour GetRandomValue

void MapGenerator::GenerateIsland(WorldMap& map, EntityManager& em, const TileRegistry& tileReg, const BiomeRegistry& biomeReg,
                                  const EnvironmentRegistry& envReg, unsigned int seed) {
    PerlinNoise elevationNoise(seed);
    PerlinNoise temperatureNoise(seed + 100);
    PerlinNoise humidityNoise(seed + 200);

    int width = map.GetWidth();
    int height = map.GetHeight();
    float centerX = width / 2.0f;
    float centerY = height / 2.0f;
    float maxRadius = std::min(centerX, centerY) * 0.9f;

    int deepWaterId = tileReg.GetTileIdByString("DEEP_WATER");
    int shallowWaterId = tileReg.GetTileIdByString("SHALLOW_WATER");

    // ==========================================
    // PASS 1 : CLIMATE GENERATION
    // ==========================================
    std::vector<float> tempMap(width * height, 0.0f);
    std::vector<float> humMap(width * height, 0.0f);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float nx = (float)x / 60.0f;
            float ny = (float)y / 60.0f;

            float elevation = elevationNoise.GetNoise(nx, ny) * 1.0f + elevationNoise.GetNoise(nx * 2.5f, ny * 2.5f) * 0.5f;
            elevation = (elevation + 1.5f) / 3.0f;
            float distToCenter = std::sqrt((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY));
            float mask = distToCenter / maxRadius;
            elevation -= (mask * mask);

            if (elevation < 0.25f) {
                map.SetTile(x, y, deepWaterId);
                continue;
            }
            if (elevation < 0.35f) {
                map.SetTile(x, y, shallowWaterId);
                continue;
            }

            float temp = (temperatureNoise.GetNoise(nx * 0.8f, ny * 0.8f) + 1.0f) * 25.0f;
            float hum = (humidityNoise.GetNoise(nx * 1.2f, ny * 1.2f) + 1.0f) * 50.0f;

            int index = y * width + x;
            tempMap[index] = temp;
            humMap[index] = hum;

            const BiomeDef* biome = biomeReg.GetClimateBiome(temp, hum);
            if (biome) {
                map.SetTile(x, y, biome->baseTileId);
            } else {
                map.SetTile(x, y, tileReg.GetTileIdByString("SAND"));
            }
        }
    }

    // ==========================================
    // PASS 2 : PATCH BIOMES (Oasis, Volcano...)
    // ==========================================
    SetRandomSeed(seed);

    for (const auto& patch : biomeReg.GetPatchBiomes()) {
        int instanceCount = GetRandomValue(patch.minInstances, patch.maxInstances);

        for (int i = 0; i < instanceCount; ++i) {
            bool placed = false;
            int attempts = 0;

            while (!placed && attempts < 100) {
                int px = GetRandomValue(0, width - 1);
                int py = GetRandomValue(0, height - 1);
                int index = py * width + px;

                int currentTile = map.GetTile(px, py);
                if (currentTile == deepWaterId || currentTile == shallowWaterId) {
                    attempts++;
                    continue;
                }

                float t = tempMap[index];
                float h = humMap[index];

                if (t >= patch.minTemp && t <= patch.maxTemp && h >= patch.minHum && h <= patch.maxHum) {
                    int radius = GetRandomValue(patch.minRadius, patch.maxRadius);
                    for (int cy = py - radius; cy <= py + radius; ++cy) {
                        for (int cx = px - radius; cx <= px + radius; ++cx) {
                            if (std::sqrt((cx - px) * (cx - px) + (cy - py) * (cy - py)) <= radius) {
                                int tId = map.GetTile(cx, cy);
                                if (tId != deepWaterId && tId != shallowWaterId) {
                                    map.SetTile(cx, cy, patch.baseTileId);
                                }
                            }
                        }
                    }
                    placed = true;
                }
                attempts++;
            }
        }
    }

    // ==========================================
    // PASS 3 : VÉGÉTATION DATA-DRIVEN
    // ==========================================
    PerlinNoise vegetationNoise(seed + 300);

    // On met en cache la correspondance (Tile ID -> BiomeDef) pour aller très vite
    std::unordered_map<int, const BiomeDef*> tileToBiome;

    // On lit tes biomes climatiques
    for (const auto& biome : biomeReg.GetClimateBiomes()) {
        tileToBiome[biome.baseTileId] = &biome;
    }
    // On lit tes biomes patch (Oasis, Volcans...)
    for (const auto& biome : biomeReg.GetPatchBiomes()) {
        tileToBiome[biome.baseTileId] = &biome;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int tileId = map.GetTile(x, y);

            // Si cette tuile appartient à un biome qu'on connaît
            auto it = tileToBiome.find(tileId);
            if (it != tileToBiome.end()) {
                const BiomeDef* biome = it->second;

                // S'il n'y a pas de flore dans ce biome, on passe
                if (biome->flora.empty())
                    continue;

                float nx = (float)x / 15.0f;
                float ny = (float)y / 15.0f;
                float vegValue = (vegetationNoise.GetNoise(nx, ny) + 1.0f) / 2.0f; // [0.0 à 1.0]

                Vector2 worldPos = {(float)x * Config::TILE_SIZE + (Config::TILE_SIZE / 2.0f),
                                    (float)y * Config::TILE_SIZE + (Config::TILE_SIZE / 2.0f)};

                // On vérifie la flore (dans l'ordre du .stv : le plus rare en premier)
                for (const auto& fDef : biome->flora) {
                    if (vegValue > fDef.noiseThreshold) {
                        envReg.SpawnEnvironment(em, fDef.prefabId, worldPos);
                        break; // On ne fait pousser qu'une seule chose par case !
                    }
                }
            }
        }
    }
}
