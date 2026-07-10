#include "world/MapGenerator.hpp"

#include "data/BiomeRegistry.hpp"
#include "data/TileRegistry.hpp"
#include "world/PerlinNoise.hpp"

#include <cmath>
#include <raylib.h> // Pour GetRandomValue

void MapGenerator::GenerateIsland(WorldMap& map, const TileRegistry& tileReg, const BiomeRegistry& biomeReg, unsigned int seed) {
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
}
