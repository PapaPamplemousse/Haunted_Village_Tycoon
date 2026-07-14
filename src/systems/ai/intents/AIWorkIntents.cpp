/**
 * @file AIWorkIntents.cpp
 * @brief Work-related AI intents: build, dismantle, repair, harvest.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/AISystemUtils.hpp"
#include "systems/Pathfinder.hpp"
#include "systems/ai/AIIntentFinders.hpp"

#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace {

bool IsValidActor(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasTransform[entity] && em.hasBehavior[entity];
}

const BehaviorRule* FindRule(const BehaviorComponent& behavior, const std::string& ruleName) {
    for (const BehaviorRule& rule : behavior.innateBehaviorRules) {
        if (rule.name == ruleName) {
            return &rule;
        }
    }

    return nullptr;
}

bool RuleTargetsPrefab(const BehaviorRule& rule, const std::string& prefabId) {
    for (const std::string& arg : rule.arguments) {
        if (arg == prefabId) {
            return true;
        }
    }

    return false;
}

bool HasAdjacentPath(EntityID entity, EntityID target, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg) {
    if (target >= em.active.size() || !em.active[target] || !em.hasTransform[target]) {
        return false;
    }

    const std::vector<Vector2> path =
        Pathfinder::FindPathToAdjacentTile(em.transforms[entity].position, em.transforms[target].position, map, tileReg, em, entity);

    return !path.empty();
}

EntityID FindNearestBuildTarget(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                const EntitySpatialGrid& spatialGrid) {
    const float radius = AISystemUtils::GetActionRadiusWorld(entity, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, radius, em);

    EntityID best = static_cast<EntityID>(-1);
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (candidate >= em.active.size() || !em.active[candidate] || !em.hasTransform[candidate] || !em.hasBlueprint[candidate] ||
            em.blueprints[candidate].isFinished) {
            continue;
        }

        if (!AISystemUtils::HasAccessibleMaterials(entity, em, spatialGrid, em.blueprints[candidate].requiredMaterials)) {
            continue;
        }

        if (!HasAdjacentPath(entity, candidate, em, map, tileReg)) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            best = candidate;
        }
    }

    return best;
}

EntityID FindNearestDismantleTarget(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                    const EntitySpatialGrid& spatialGrid) {
    const float radius = AISystemUtils::GetActionRadiusWorld(entity, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, radius, em);

    EntityID best = static_cast<EntityID>(-1);
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (candidate >= em.active.size() || !em.active[candidate] || !em.hasTransform[candidate] || !em.hasDeconstruct[candidate]) {
            continue;
        }

        if (!HasAdjacentPath(entity, candidate, em, map, tileReg)) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            best = candidate;
        }
    }

    return best;
}

EntityID FindNearestRepairTarget(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                 const EntitySpatialGrid& spatialGrid) {
    const float radius = AISystemUtils::GetActionRadiusWorld(entity, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, radius, em);

    EntityID best = static_cast<EntityID>(-1);
    float bestDamageRatio = 0.0f;
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (candidate >= em.active.size() || !em.active[candidate] || !em.hasTransform[candidate] || !em.hasHealth[candidate]) {
            continue;
        }

        if (candidate == entity) {
            continue;
        }

        if (em.healths[candidate].max <= 0.0f || em.healths[candidate].current >= em.healths[candidate].max) {
            continue;
        }

        if (!HasAdjacentPath(entity, candidate, em, map, tileReg)) {
            continue;
        }

        const float damageRatio = 1.0f - em.healths[candidate].current / em.healths[candidate].max;

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        if (damageRatio > bestDamageRatio || (damageRatio == bestDamageRatio && distanceSq < bestDistanceSq)) {
            bestDamageRatio = damageRatio;
            bestDistanceSq = distanceSq;
            best = candidate;
        }
    }

    return best;
}

EntityID FindNearestHarvestTarget(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                  const EntitySpatialGrid& spatialGrid) {
    if (!em.hasBehavior[entity]) {
        return static_cast<EntityID>(-1);
    }

    const BehaviorRule* harvestRule = FindRule(em.behaviors[entity], "harvest");

    if (harvestRule == nullptr || harvestRule->arguments.empty()) {
        return static_cast<EntityID>(-1);
    }

    const float radius = AISystemUtils::GetActionRadiusWorld(entity, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, radius, em);

    EntityID best = static_cast<EntityID>(-1);
    float bestScore = -1.0f;

    for (EntityID candidate : candidates) {
        if (candidate >= em.active.size() || !em.active[candidate] || !em.hasTransform[candidate] || !em.hasHarvestable[candidate] ||
            !em.hasHealth[candidate] || !em.hasTag[candidate]) {
            continue;
        }

        if (em.hasBlueprint[candidate] && !em.blueprints[candidate].isFinished) {
            continue;
        }

        if (!RuleTargetsPrefab(*harvestRule, em.tags[candidate].prefabId)) {
            continue;
        }

        const HarvestableComponent& harvestable = em.harvestables[candidate];

        if (em.healths[candidate].current <= 0.0f) {
            continue;
        }

        if (!AISystemUtils::HasRequiredHarvestTool(entity, harvestable, em)) {
            continue;
        }

        if (!HasAdjacentPath(entity, candidate, em, map, tileReg)) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        const float score = 100.0f - std::sqrt(distanceSq) * 0.01f;

        if (score > bestScore) {
            bestScore = score;
            best = candidate;
        }
    }

    return best;
}

} // namespace

namespace ai::intents {

std::optional<AIIntent> FindBuildIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                        const ResourceRegistry&, const WeaponRegistry&, const EntitySpatialGrid& spatialGrid) {
    if (!IsValidActor(em, entity)) {
        return std::nullopt;
    }

    const EntityID target = FindNearestBuildTarget(entity, em, map, tileReg, spatialGrid);

    if (target == static_cast<EntityID>(-1)) {
        return std::nullopt;
    }

    AIIntent intent;
    intent.kind = AIIntentKind::MoveAdjacentToEntity;
    intent.targetEntity = target;
    intent.moveTask = "moving_to_build";
    intent.actionTask = "building";
    intent.actionDuration = 1.0f;

    return intent;
}

std::optional<AIIntent> FindDismantleIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                            const ResourceRegistry&, const WeaponRegistry&, const EntitySpatialGrid& spatialGrid) {
    if (!IsValidActor(em, entity)) {
        return std::nullopt;
    }

    const EntityID target = FindNearestDismantleTarget(entity, em, map, tileReg, spatialGrid);

    if (target == static_cast<EntityID>(-1)) {
        return std::nullopt;
    }

    AIIntent intent;
    intent.kind = AIIntentKind::MoveAdjacentToEntity;
    intent.targetEntity = target;
    intent.moveTask = "moving_to_dismantle";
    intent.actionTask = "dismantling";
    intent.actionDuration = 1.0f;

    return intent;
}

std::optional<AIIntent> FindRepairIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                         const ResourceRegistry&, const WeaponRegistry&, const EntitySpatialGrid& spatialGrid) {
    if (!IsValidActor(em, entity)) {
        return std::nullopt;
    }

    const EntityID target = FindNearestRepairTarget(entity, em, map, tileReg, spatialGrid);

    if (target == static_cast<EntityID>(-1)) {
        return std::nullopt;
    }

    AIIntent intent;
    intent.kind = AIIntentKind::MoveAdjacentToEntity;
    intent.targetEntity = target;
    intent.moveTask = "moving_to_repair";
    intent.actionTask = "repairing";
    intent.actionDuration = 1.0f;

    return intent;
}

std::optional<AIIntent> FindHarvestIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                          const ResourceRegistry&, const WeaponRegistry&, const EntitySpatialGrid& spatialGrid) {
    if (!IsValidActor(em, entity)) {
        return std::nullopt;
    }

    const EntityID target = FindNearestHarvestTarget(entity, em, map, tileReg, spatialGrid);

    if (target == static_cast<EntityID>(-1)) {
        return std::nullopt;
    }

    AIIntent intent;
    intent.kind = AIIntentKind::MoveAdjacentToEntity;
    intent.targetEntity = target;
    intent.moveTask = "moving_to_harvest";
    intent.actionTask = "harvesting";
    intent.actionDuration = 1.0f;

    return intent;
}

} // namespace ai::intents
