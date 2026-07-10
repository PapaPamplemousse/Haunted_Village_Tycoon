#include "systems/RoomSystem.hpp"

#include "core/Config.hpp"

#include <iostream>
#include <queue>
#include <unordered_map>
#include <vector>

void RoomSystem::Update(EntityManager& em, const WorldMap& map, const StructureRegistry& structReg) {
    m_justRecalculated = false;

    // On ne recalcule QUE si un mur/meuble fini a réellement changé.
    if (!m_isDirty) {
        return;
    }

    m_isDirty = false;
    m_justRecalculated = true;

    int width = map.GetWidth();
    int height = map.GetHeight();

    // 1. Nettoyage : on détruit toutes les anciennes entités pièces.
    for (size_t i = 0; i < em.active.size(); ++i) {
        if (em.active[i] && em.hasRoom[i]) {
            em.DestroyEntity(i);
        }
    }

    // 2. Grilles rapides : murs/portes finis et meubles finis.
    std::vector<bool> isWallGrid(width * height, false);
    std::vector<std::string> furnitureGrid(width * height, "");

    for (size_t i = 0; i < em.active.size(); ++i) {
        if (!em.active[i] || !em.hasTransform[i]) {
            continue;
        }

        // IMPORTANT:
        // Les blueprints non finis ne doivent pas compter pour les pièces.
        // Donc:
        // - un mur blueprint ne ferme pas une pièce;
        // - une porte blueprint ne ferme pas une pièce;
        // - un meuble blueprint ne valide pas les requirements d'une structure.
        if (em.hasBlueprint[i] && !em.blueprints[i].isFinished) {
            continue;
        }

        int gx = static_cast<int>(em.transforms[i].position.x / Config::TILE_SIZE);
        int gy = static_cast<int>(em.transforms[i].position.y / Config::TILE_SIZE);

        if (gx < 0 || gx >= width || gy < 0 || gy >= height) {
            continue;
        }

        int idx = gy * width + gx;

        // Mur ou porte finis : ça ferme une pièce.
        if (em.hasConstruction[i] && (em.constructions[i].isWall || em.constructions[i].isDoor)) {
            isWallGrid[idx] = true;
        }
        // Meuble fini : compte dans les requirements de structure.
        else if (em.hasTag[i] && em.hasSprite[i] && !em.hasConstruction[i] && !em.hasBehavior[i]) {
            furnitureGrid[idx] = em.tags[i].prefabId;
        }
    }

    // 3. Flood-fill.
    std::vector<bool> visited(width * height, false);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int startIndex = y * width + x;

            if (visited[startIndex] || isWallGrid[startIndex]) {
                continue;
            }

            std::queue<int> q;
            q.push(startIndex);
            visited[startIndex] = true;

            bool isEnclosed = true;
            int area = 0;
            std::vector<Vector2> roomTiles;
            std::unordered_map<std::string, int> roomContents;

            while (!q.empty()) {
                int curr = q.front();
                q.pop();

                int cx = curr % width;
                int cy = curr / width;

                area++;
                roomTiles.push_back({(float)cx, (float)cy});

                if (!furnitureGrid[curr].empty()) {
                    roomContents[furnitureGrid[curr]]++;
                }

                if (cx == 0 || cx == width - 1 || cy == 0 || cy == height - 1) {
                    isEnclosed = false;
                }

                int neighbors[4] = {curr - width, curr + width, curr - 1, curr + 1};

                int nx[4] = {cx, cx, cx - 1, cx + 1};

                int ny[4] = {cy - 1, cy + 1, cy, cy};

                for (int n = 0; n < 4; ++n) {
                    if (nx[n] >= 0 && nx[n] < width && ny[n] >= 0 && ny[n] < height) {
                        int nIdx = neighbors[n];

                        if (!visited[nIdx] && !isWallGrid[nIdx]) {
                            visited[nIdx] = true;
                            q.push(nIdx);
                        }
                    }
                }
            }

            // 4. Evaluation de la pièce.
            if (isEnclosed && area > 0) {
                const StructureDef* bestMatch = nullptr;
                int bestScore = -1;

                for (const auto& pair : structReg.GetAllStructures()) {
                    const StructureDef& def = pair.second;

                    if (area < def.minArea || area > def.maxArea) {
                        continue;
                    }

                    bool hasAllRequirements = true;

                    for (const auto& req : def.requirements) {
                        if (roomContents[req.first] < req.second) {
                            hasAllRequirements = false;
                            break;
                        }
                    }

                    if (hasAllRequirements) {
                        int score = static_cast<int>(def.requirements.size());

                        if (score > bestScore) {
                            bestScore = score;
                            bestMatch = &def;
                        }
                    }
                }

                EntityID roomId = em.CreateEntity();
                em.hasRoom[roomId] = true;

                if (bestMatch) {
                    em.rooms[roomId] = {bestMatch->id, bestMatch->name, area, roomTiles};

                    if (!bestMatch->jobSlots.empty()) {
                        em.hasWorkplace[roomId] = true;
                        em.workplaces[roomId] = {};

                        for (const auto& job : bestMatch->jobSlots) {
                            for (int k = 0; k < job.second; ++k) {
                                em.workplaces[roomId].slots.push_back({job.first, static_cast<EntityID>(-1)});
                            }
                        }
                    }

                    std::cout << "[ROOM] Detected: " << bestMatch->name << " (Area: " << area << ")" << std::endl;
                } else {
                    em.rooms[roomId] = {"EMPTY_ROOM", "Empty Room", area, roomTiles};

                    std::cout << "[ROOM] Detected: Empty Room (Area: " << area << ")" << std::endl;
                }
            }
        }
    }
}
