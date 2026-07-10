#include "systems/AISystem.hpp"

#include "core/Config.hpp"

#include <cmath>
#include <raymath.h>

void AISystem::Update(float deltaTime, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg, RoomSystem& roomSys) {
    for (size_t i = 0; i < em.active.size(); ++i) {
        if (!em.active[i] || !em.hasBehavior[i] || !em.hasTransform[i] || !em.hasStats[i])
            continue;

        auto& behavior = em.behaviors[i];

        // 1. Gestion du temps de pause
        if (behavior.stateTimer > 0.0f) {
            behavior.stateTimer -= deltaTime;
            continue;
        }

        // 2. Routage vers le bon État
        if (behavior.currentTask == "idle") {
            HandleIdleState(i, em, map, tileReg);
        } else if (behavior.isMoving) {
            HandleMovingState(i, deltaTime, em);
        } else {
            // Si on ne fait rien, qu'on ne bouge pas, c'est qu'on est en train d'exécuter une action
            HandleTaskCompletion(i, em, roomSys);
        }
    }
}

// ============================================================================
// STATE HANDLERS
// ============================================================================

void AISystem::HandleIdleState(EntityID i, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg) {
    auto& behavior = em.behaviors[i];

    // On extrait les capacités de l'entité
    bool canBuild = false, canDismantle = false, canWander = false;
    for (const auto& cap : behavior.innateCapabilities) {
        if (cap == "build")
            canBuild = true;
        if (cap == "dismantle")
            canDismantle = true;
        if (cap == "wander")
            canWander = true;
    }

    // Chaîne de priorité des Jobs (On s'arrête dès qu'on en trouve un)
    if (canBuild && TryFindBuildJob(i, em))
        return;
    if (canDismantle && TryFindDismantleJob(i, em))
        return;
    if (canWander && TryFindWanderJob(i, em, map, tileReg))
        return;
}

void AISystem::HandleMovingState(EntityID i, float deltaTime, EntityManager& em) {
    auto& behavior = em.behaviors[i];
    auto& transform = em.transforms[i];
    float speed = em.stats[i].maxSpeed;

    Vector2 dir = Vector2Subtract(behavior.currentTarget, transform.position);
    float distanceToTarget = Vector2Length(dir);

    // Arrivé à destination ?
    if (distanceToTarget < Config::TILE_SIZE * 0.8f) {
        behavior.isMoving = false;

        // Transition vers l'action
        if (behavior.currentTask == "moving_to_build") {
            behavior.currentTask = "building";
            behavior.stateTimer = 2.0f; // Temps de construction
        } else if (behavior.currentTask == "moving_to_dismantle") {
            behavior.currentTask = "dismantling";
            behavior.stateTimer = 2.0f; // Temps de démolition
        } else if (behavior.currentTask == "wandering") {
            behavior.currentTask = "idle";
            behavior.stateTimer = GetRandomValue(10, 40) / 10.0f; // Pause avant de repartir
        }
    } else {
        // Avancer
        Vector2 normalizedDir = Vector2Scale(dir, 1.0f / distanceToTarget);
        transform.position.x += normalizedDir.x * speed * deltaTime;
        transform.position.y += normalizedDir.y * speed * deltaTime;
    }
}

void AISystem::HandleTaskCompletion(EntityID i, EntityManager& em, RoomSystem& roomSys) {
    auto& behavior = em.behaviors[i];

    if (behavior.currentTask == "building") {
        EntityID target = behavior.currentJobTarget;
        if (em.active[target] && em.hasBlueprint[target]) {
            // Consommer les ressources
            if (em.hasInventory[i]) {
                for (const auto& req : em.blueprints[target].requiredMaterials) {
                    em.inventories[i].items[req.first] -= req.second;
                }
            }
            // Finaliser
            em.blueprints[target].isFinished = true;
            em.hasBlueprint[target] = false;

            // --- MISE A JOUR DES PIECES DEMANDEE ---
            if (em.hasConstruction[target] || (em.hasTag[target] && !em.hasBehavior[target])) {
                roomSys.MarkDirty();
            }
        }
    } else if (behavior.currentTask == "dismantling") {
        EntityID target = behavior.currentJobTarget;
        if (em.active[target] && em.hasDeconstruct[target]) {
            // Rembourser
            if (em.hasCost[target] && em.hasInventory[i]) {
                for (const auto& req : em.costs[target].materials) {
                    int refund = std::max(1, req.second / 2);
                    em.inventories[i].items[req.first] += refund;
                }
            }
            // Mise à jour des pièces et destruction
            if (em.hasConstruction[target] || (em.hasTag[target] && !em.hasBehavior[target])) {
                roomSys.MarkDirty();
            }
            em.DestroyEntity(target);
        }
    }

    // Retour à zéro
    behavior.currentTask = "idle";
    behavior.hasJob = false;
}

// ============================================================================
// JOB SEARCHERS
// ============================================================================

bool AISystem::TryFindBuildJob(EntityID i, EntityManager& em) {
    for (size_t j = 0; j < em.active.size(); ++j) {
        if (em.active[j] && em.hasBlueprint[j] && !em.blueprints[j].isFinished && em.hasTransform[j]) {
            bool canAfford = true;
            if (em.hasInventory[i]) {
                for (const auto& req : em.blueprints[j].requiredMaterials) {
                    if (em.inventories[i].items[req.first] < req.second) {
                        canAfford = false;
                        break;
                    }
                }
            }
            if (canAfford) {
                auto& behavior = em.behaviors[i];
                behavior.currentTask = "moving_to_build";
                behavior.currentJobTarget = j;
                behavior.hasJob = true;
                behavior.currentTarget = em.transforms[j].position;
                behavior.isMoving = true;
                return true;
            }
        }
    }
    return false;
}

bool AISystem::TryFindDismantleJob(EntityID i, EntityManager& em) {
    for (size_t j = 0; j < em.active.size(); ++j) {
        if (em.active[j] && em.hasDeconstruct[j] && em.hasTransform[j]) {
            auto& behavior = em.behaviors[i];
            behavior.currentTask = "moving_to_dismantle";
            behavior.currentJobTarget = j;
            behavior.hasJob = true;
            behavior.currentTarget = em.transforms[j].position;
            behavior.isMoving = true;
            return true;
        }
    }
    return false;
}

bool AISystem::TryFindWanderJob(EntityID i, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg) {
    auto& behavior = em.behaviors[i];
    float angle = GetRandomValue(0, 360) * DEG2RAD;
    float distance = GetRandomValue(Config::TILE_SIZE * 2, Config::TILE_SIZE * 8);

    Vector2 proposedTarget = {em.transforms[i].position.x + std::cos(angle) * distance,
                              em.transforms[i].position.y + std::sin(angle) * distance};

    int gridX = static_cast<int>(proposedTarget.x / Config::TILE_SIZE);
    int gridY = static_cast<int>(proposedTarget.y / Config::TILE_SIZE);

    const TileDef* tileDef = tileReg.GetTileDef(map.GetTile(gridX, gridY));
    if (tileDef && tileDef->walkable) {
        behavior.currentTask = "wandering";
        behavior.currentTarget = proposedTarget;
        behavior.isMoving = true;
        return true;
    } else {
        behavior.stateTimer = 1.0f;
        return false;
    }
}
