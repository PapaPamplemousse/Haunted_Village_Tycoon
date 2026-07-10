#include "systems/AISystem.hpp"

#include "core/Config.hpp"
#include "systems/RoomSystem.hpp"

#include <cmath>
#include <raymath.h>

void AISystem::Update(float deltaTime, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg, RoomSystem& roomSys) {
    for (size_t i = 0; i < em.active.size(); ++i) {
        if (!em.active[i] || !em.hasBehavior[i] || !em.hasTransform[i] || !em.hasStats[i])
            continue;

        auto& behavior = em.behaviors[i];
        auto& transform = em.transforms[i];
        const auto& stats = em.stats[i];

        if (behavior.stateTimer > 0.0f) {
            behavior.stateTimer -= deltaTime;
            continue;
        }

        // ==========================================
        // 1. RECHERCHE DE TÂCHE
        // ==========================================
        if (behavior.currentTask == "idle") {
            bool canBuild = false;
            bool canDismantle = false;
            bool canWander = false;

            for (const auto& cap : behavior.innateCapabilities) {
                if (cap == "build")
                    canBuild = true;
                if (cap == "dismantle")
                    canDismantle = true;
                if (cap == "wander")
                    canWander = true;
            }

            // A. Priorité 1 : Chercher un Blueprint à construire
            if (canBuild) {
                for (size_t j = 0; j < em.active.size(); ++j) {
                    // Si on trouve un Blueprint non terminé
                    if (em.active[j] && em.hasBlueprint[j] && !em.blueprints[j].isFinished && em.hasTransform[j]) {
                        // VERIFICATION DES RESSOURCES (Inventaire du PNJ)
                        bool canAfford = true;
                        if (em.hasInventory[i]) {
                            for (const auto& req : em.blueprints[j].requiredMaterials) {
                                if (em.inventories[i].items[req.first] < req.second) {
                                    canAfford = false;
                                    break;
                                }
                            }
                        }

                        // Si on a les ressources, on prend le job !
                        if (canAfford) {
                            behavior.currentTask = "moving_to_build";
                            behavior.currentJobTarget = j;
                            behavior.hasJob = true;
                            behavior.currentTarget = em.transforms[j].position;
                            behavior.isMoving = true;
                            break; // On arrête de chercher
                        }
                    }
                }
            }

            // B. Priorité 2 : Chercher un objet à démolir
            if (!behavior.hasJob && canDismantle) {
                for (size_t j = 0; j < em.active.size(); ++j) {
                    if (em.active[j] && em.hasDeconstruct[j] && em.hasTransform[j]) {
                        behavior.currentTask = "moving_to_dismantle";
                        behavior.currentJobTarget = j;
                        behavior.hasJob = true;
                        behavior.currentTarget = em.transforms[j].position;
                        behavior.isMoving = true;
                        break;
                    }
                }
            }

            // C. Priorité 3 : Si aucun job trouvé, on flâne
            if (!behavior.hasJob && canWander) {
                float angle = GetRandomValue(0, 360) * DEG2RAD;
                float distance = GetRandomValue(Config::TILE_SIZE * 2, Config::TILE_SIZE * 8);
                Vector2 proposedTarget = {transform.position.x + std::cos(angle) * distance,
                                          transform.position.y + std::sin(angle) * distance};

                int gridX = static_cast<int>(proposedTarget.x / Config::TILE_SIZE);
                int gridY = static_cast<int>(proposedTarget.y / Config::TILE_SIZE);

                const TileDef* tileDef = tileReg.GetTileDef(map.GetTile(gridX, gridY));
                if (tileDef && tileDef->walkable) {
                    behavior.currentTask = "wandering";
                    behavior.currentTarget = proposedTarget;
                    behavior.isMoving = true;
                } else {
                    behavior.stateTimer = 1.0f; // On attend avant de réessayer
                }
            }
        }

        // ==========================================
        // 2. EXÉCUTION DE LA TÂCHE
        // ==========================================
        if (behavior.isMoving) {
            Vector2 dir = Vector2Subtract(behavior.currentTarget, transform.position);
            float distanceToTarget = Vector2Length(dir);

            // On s'arrête quand on est sur la case adjacente (Marge d'erreur de TILE_SIZE/2)
            if (distanceToTarget < Config::TILE_SIZE * 0.8f) {
                behavior.isMoving = false;

                if (behavior.currentTask == "moving_to_build") {
                    behavior.currentTask = "building";
                    behavior.stateTimer = 2.0f; // Il met 2 secondes à construire
                } else if (behavior.currentTask == "moving_to_dismantle") {
                    behavior.currentTask = "dismantling";
                    behavior.stateTimer = 2.0f; // Il met 2 secondes à détruire !
                } else if (behavior.currentTask == "wandering") {
                    behavior.currentTask = "idle";
                    behavior.stateTimer = GetRandomValue(10, 40) / 10.0f;
                }
            } else {
                Vector2 normalizedDir = Vector2Scale(dir, 1.0f / distanceToTarget);
                transform.position.x += normalizedDir.x * stats.maxSpeed * deltaTime;
                transform.position.y += normalizedDir.y * stats.maxSpeed * deltaTime;
            }
        }

        // ==========================================
        // 3. FINALISATION (Fin du délai de travail)
        // ==========================================
        else if (behavior.currentTask == "building") {
            EntityID target = behavior.currentJobTarget;

            // On vérifie que le blueprint existe toujours
            if (em.active[target] && em.hasBlueprint[target]) {
                // Consommer les ressources de l'inventaire du PNJ
                if (em.hasInventory[i]) {
                    for (const auto& req : em.blueprints[target].requiredMaterials) {
                        em.inventories[i].items[req.first] -= req.second;
                    }
                }

                // Finaliser la construction !
                em.blueprints[target].isFinished = true;
                em.hasBlueprint[target] = false; // Ce n'est plus un fantôme !
            }
            // Retour à la vie normale
            behavior.currentTask = "idle";
            behavior.hasJob = false;
        } else if (behavior.currentTask == "dismantling") {
            EntityID target = behavior.currentJobTarget;

            if (em.active[target] && em.hasDeconstruct[target]) {
                // 1. Rembourser la moitié du coût !
                if (em.hasCost[target] && em.hasInventory[i]) {
                    for (const auto& req : em.costs[target].materials) {
                        int refund = std::max(1, req.second / 2); // Au moins 1 de récupéré
                        em.inventories[i].items[req.first] += refund;
                    }
                }

                // 2. Notifier le RoomSystem si c'était un mur ou un meuble
                if (em.hasConstruction[target] || (em.hasTag[target] && !em.hasBehavior[target])) {
                    roomSys.MarkDirty();
                }

                // 3. Destruction pure et simple !
                em.DestroyEntity(target);
            }
            behavior.currentTask = "idle";
            behavior.hasJob = false;
        }
    }
}
