#include "systems/RoomSystem.hpp"

#include "core/Config.hpp"

#include <iostream>
#include <queue>

void RoomSystem::Update(EntityManager& em, const WorldMap& map, const StructureRegistry& structReg) {
    m_justRecalculated = false;

    // On ne recalcule QUE si un mur a changé (Évite les lags)
    if (!m_isDirty)
        return;
    m_isDirty = false;
    m_justRecalculated = true;

    int width = map.GetWidth();
    int height = map.GetHeight();

    // 1. Nettoyage : On détruit TOUTES les anciennes pièces
    for (size_t i = 0; i < em.active.size(); ++i) {
        if (em.active[i] && em.hasRoom[i]) {
            em.DestroyEntity(i);
        }
    }

    // 2. Préparation : On crée une carte virtuelle rapide des murs et des meubles
    std::vector<bool> isWallGrid(width * height, false);
    std::vector<std::string> furnitureGrid(width * height, "");

    for (size_t i = 0; i < em.active.size(); ++i) {
        if (!em.active[i] || !em.hasTransform[i])
            continue;

        int gx = static_cast<int>(em.transforms[i].position.x / Config::TILE_SIZE);
        int gy = static_cast<int>(em.transforms[i].position.y / Config::TILE_SIZE);
        if (gx < 0 || gx >= width || gy < 0 || gy >= height)
            continue;

        int idx = gy * width + gx;

        // C'est un Mur ou une Porte : ça ferme une pièce
        if (em.hasConstruction[i] && (em.constructions[i].isWall || em.constructions[i].isDoor)) {
            isWallGrid[idx] = true;
        }
        // C'est un Meuble (A un Tag, un Sprite, mais PAS de construction ni d'IA)
        else if (em.hasTag[i] && em.hasSprite[i] && !em.hasConstruction[i] && !em.hasBehavior[i]) {
            furnitureGrid[idx] = em.tags[i].prefabId;
        }
    }

    // 3. Flood-Fill (Recherche de zones fermées)
    std::vector<bool> visited(width * height, false);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int startIndex = y * width + x;

            // On ignore les murs et les cases déjà visitées
            if (visited[startIndex] || isWallGrid[startIndex])
                continue;

            // Début d'un nouveau Flood-Fill
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

                // On relève les meubles présents sur cette case
                if (!furnitureGrid[curr].empty()) {
                    roomContents[furnitureGrid[curr]]++;
                }

                // Si on touche le bord de la carte, c'est l'extérieur (pas une pièce !)
                if (cx == 0 || cx == width - 1 || cy == 0 || cy == height - 1) {
                    isEnclosed = false;
                }

                // Propagation aux 4 voisins (Haut, Bas, Gauche, Droite)
                int neighbors[4] = {curr - width, curr + width, curr - 1, curr + 1};
                int nx[4] = {cx, cx, cx - 1, cx + 1};
                int ny[4] = {cy - 1, cy + 1, cy, cy};

                for (int i = 0; i < 4; ++i) {
                    if (nx[i] >= 0 && nx[i] < width && ny[i] >= 0 && ny[i] < height) {
                        int nIdx = neighbors[i];
                        if (!visited[nIdx] && !isWallGrid[nIdx]) {
                            visited[nIdx] = true;
                            q.push(nIdx);
                        }
                    }
                }
            } // Fin du While (La pièce est entièrement scannée)

            // 4. Évaluation de la pièce si elle est fermée
            if (isEnclosed && area > 0) {
                const StructureDef* bestMatch = nullptr;
                int bestScore = -1; // Le score = le nombre d'objets requis

                for (const auto& pair : structReg.GetAllStructures()) {
                    const StructureDef& def = pair.second;

                    // Vérification de la taille (CORRIGE LE BUG 1)
                    if (area < def.minArea || area > def.maxArea)
                        continue;

                    // Vérification du contenu
                    bool hasAllRequirements = true;
                    for (const auto& req : def.requirements) {
                        if (roomContents[req.first] < req.second) {
                            hasAllRequirements = false;
                            break;
                        }
                    }

                    // Calcul du score (CORRIGE LE BUG 2 : FORSAKEN RUIN vs TENT)
                    if (hasAllRequirements) {
                        int score = def.requirements.size();
                        if (score > bestScore) {
                            bestScore = score;
                            bestMatch = &def;
                        }
                    }
                }

                // Création de l'Entité Pièce
                if (bestMatch) {
                    EntityID roomId = em.CreateEntity();
                    em.hasRoom[roomId] = true;
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
                    // C'est une pièce fermée, mais elle ne valide aucune recette
                    EntityID roomId = em.CreateEntity();
                    em.hasRoom[roomId] = true;
                    em.rooms[roomId] = {"EMPTY_ROOM", "Empty Room", area, roomTiles};
                    std::cout << "[ROOM] Detected: Empty Room (Area: " << area << ")" << std::endl;
                }
            }
        }
    }
}
