#include "core/Config.hpp"
#include "systems/AISystem.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/Pathfinder.hpp"

#include <cmath>
#include <limits>
#include <raymath.h>
#include <utility>
#include <vector>

bool AISystem::TryFindHuntJob(EntityID hunter, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                              const EntitySpatialGrid& spatialGrid) {
    // Vérification initiale de l'entité chasseur
    if (hunter >= em.active.size() || !em.active[hunter] || !em.hasBehavior[hunter] || !em.hasTransform[hunter]) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[hunter];

    const BehaviorRule* huntRule = FindBehaviorRule(behavior, "hunt");

    if (huntRule == nullptr || huntRule->arguments.empty()) {
        return false;
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(hunter, em);
    const Vector2 hunterPosition = em.transforms[hunter].position;

    // Récupération des candidats via la grid
    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(hunterPosition, searchRadius, em);

    EntityID bestTarget = static_cast<EntityID>(-1);
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID target : candidates) {
        if (target == hunter) {
            continue;
        }

        // Vérification de sécurité pour le target (ajoutée par l'autre IA)
        if (target >= em.active.size() || !em.active[target] || !em.hasTag[target] || !em.hasTransform[target] || !em.hasHealth[target]) {
            continue;
        }

        if (em.healths[target].current <= 0.0f) {
            continue;
        }

        const std::string& targetSpecies = em.tags[target].species;

        if (!IsSpeciesTargetedByRule(*huntRule, targetSpecies)) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(hunterPosition, em.transforms[target].position);

        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestTarget = target;
        }
    }

    // Si aucune cible valide trouvée
    if (bestTarget == static_cast<EntityID>(-1)) {
        return false;
    }

    // Gestion de l'attaque si adjacent
    if (AreEntitiesAdjacent(hunter, bestTarget, em)) {
        behavior.currentTask = "attacking";
        behavior.currentJobTarget = bestTarget;
        behavior.hasJob = true;
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.stateTimer = AISystemUtils::ATTACK_DURATION;
        return true;
    }

    // Calcul du chemin pour se déplacer vers la cible
    std::vector<Vector2> path =
        Pathfinder::FindPathToAdjacentTile(em.transforms[hunter].position, em.transforms[bestTarget].position, map, tileReg, em, hunter);

    if (path.empty()) {
        return false;
    }

    behavior.currentTask = "moving_to_hunt";
    behavior.currentJobTarget = bestTarget;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;

    return true;
}

bool AISystem::TryFindBuildJob(EntityID i, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                               const EntitySpatialGrid& spatialGrid) {
    if (i >= em.active.size() || !em.active[i] || !em.hasTransform[i]) {
        return false;
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(i, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[i].position, searchRadius, em);

    for (EntityID j : candidates) {
        if (j >= em.active.size() || !em.active[j] || !em.hasBlueprint[j] || em.blueprints[j].isFinished || !em.hasTransform[j]) {
            continue;
        }

        if (!AISystemUtils::HasAccessibleMaterials(i, em, spatialGrid, em.blueprints[j].requiredMaterials)) {
            continue;
        }

        std::vector<Vector2> path =
            Pathfinder::FindPathToAdjacentTile(em.transforms[i].position, em.transforms[j].position, map, tileReg, em, i);

        if (path.empty()) {
            continue;
        }

        auto& behavior = em.behaviors[i];

        behavior.currentTask = "moving_to_build";
        behavior.currentJobTarget = j;
        behavior.hasJob = true;
        behavior.currentPath = std::move(path);
        behavior.currentPathIndex = 0;
        behavior.currentTarget = behavior.currentPath[0];
        behavior.isMoving = true;

        return true;
    }

    return false;
}

bool AISystem::TryFindDismantleJob(EntityID i, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                   const EntitySpatialGrid& spatialGrid) {
    if (i >= em.active.size() || !em.active[i] || !em.hasTransform[i]) {
        return false;
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(i, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[i].position, searchRadius, em);

    for (EntityID j : candidates) {
        if (j >= em.active.size() || !em.active[j] || !em.hasDeconstruct[j] || !em.hasTransform[j]) {
            continue;
        }

        std::vector<Vector2> path =
            Pathfinder::FindPathToAdjacentTile(em.transforms[i].position, em.transforms[j].position, map, tileReg, em, i);

        if (path.empty()) {
            continue;
        }

        auto& behavior = em.behaviors[i];

        behavior.currentTask = "moving_to_dismantle";
        behavior.currentJobTarget = j;
        behavior.hasJob = true;
        behavior.currentPath = std::move(path);
        behavior.currentPathIndex = 0;
        behavior.currentTarget = behavior.currentPath[0];
        behavior.isMoving = true;

        return true;
    }

    return false;
}

bool AISystem::TryFindWanderJob(EntityID i, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg) {
    auto& behavior = em.behaviors[i];

    constexpr int MaxAttempts = 8;

    for (int attempt = 0; attempt < MaxAttempts; ++attempt) {
        const float angle = GetRandomValue(0, 360) * DEG2RAD;

        const float distance =
            static_cast<float>(GetRandomValue(static_cast<int>(Config::TILE_SIZE * 2), static_cast<int>(Config::TILE_SIZE * 8)));

        Vector2 proposedTarget = {em.transforms[i].position.x + std::cos(angle) * distance,
                                  em.transforms[i].position.y + std::sin(angle) * distance};

        std::vector<Vector2> path = Pathfinder::FindPath(em.transforms[i].position, proposedTarget, map, tileReg, em, i);

        if (path.empty()) {
            continue;
        }

        behavior.currentTask = "wandering";
        behavior.currentPath = std::move(path);
        behavior.currentPathIndex = 0;
        behavior.currentTarget = behavior.currentPath[0];
        behavior.isMoving = true;

        return true;
    }

    behavior.stateTimer = 1.0f;
    return false;
}

bool AISystem::TryFindHarvestJob(EntityID worker, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                 const EntitySpatialGrid& spatialGrid) {
    if (worker >= em.active.size() || !em.active[worker] || !em.hasBehavior[worker] || !em.hasTransform[worker]) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[worker];
    const BehaviorRule* harvestRule = FindBehaviorRule(behavior, "harvest");

    if (harvestRule == nullptr || harvestRule->arguments.empty()) {
        return false;
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(worker, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[worker].position, searchRadius, em);

    EntityID bestTarget = static_cast<EntityID>(-1);
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    const Vector2 workerPosition = em.transforms[worker].position;

    for (EntityID target : candidates) {
        if (target == worker || target >= em.active.size() || !em.active[target] || !em.hasHarvestable[target] ||
            !em.hasTransform[target] || !em.hasTag[target]) {
            continue;
        }

        const std::string& targetPrefab = em.tags[target].prefabId;

        bool isTargeted = false;

        for (const std::string& arg : harvestRule->arguments) {
            if (arg == targetPrefab) {
                isTargeted = true;
                break;
            }
        }

        if (!isTargeted) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(workerPosition, em.transforms[target].position);

        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestTarget = target;
        }
    }

    if (bestTarget == static_cast<EntityID>(-1)) {
        return false;
    }

    if (AreEntitiesAdjacent(worker, bestTarget, em)) {
        behavior.currentTask = "harvesting";
        behavior.currentJobTarget = bestTarget;
        behavior.hasJob = true;
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.stateTimer = 3.0f;
        return true;
    }

    std::vector<Vector2> path =
        Pathfinder::FindPathToAdjacentTile(workerPosition, em.transforms[bestTarget].position, map, tileReg, em, worker);

    if (path.empty()) {
        return false;
    }

    behavior.currentTask = "moving_to_harvest";
    behavior.currentJobTarget = bestTarget;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;

    return true;
}
