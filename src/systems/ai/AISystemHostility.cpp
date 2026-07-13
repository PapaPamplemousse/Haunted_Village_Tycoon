/**
 * @file AISystemHostility.cpp
 * @brief AI hostile social actions: intimidation, non-lethal fights, and extremely rare murder.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/AISystem.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/Pathfinder.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr float INTIMIDATE_MIN_RESENTMENT = 55.0f;
constexpr float FIGHT_MIN_RESENTMENT = 75.0f;
constexpr float MURDER_MIN_RESENTMENT = 95.0f;

constexpr float INTIMIDATE_MAX_FRIENDSHIP = 45.0f;
constexpr float FIGHT_MAX_FRIENDSHIP = 25.0f;
constexpr float MURDER_MAX_FRIENDSHIP = 5.0f;

constexpr float HOSTILITY_RADIUS_TILES = 12.0f;
constexpr float WITNESS_RADIUS_TILES = 6.0f;

bool IsValidHuman(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasTag[entity] && em.hasTransform[entity] && em.hasSocial[entity] &&
           em.hasHealth[entity] && em.tags[entity].species == "human";
}

bool IsAdultHuman(const EntityManager& em, EntityID entity) {
    return IsValidHuman(em, entity) && em.tags[entity].age >= Config::ADULT_AGE;
}

bool AreSameVillage(const EntityManager& em, EntityID a, EntityID b) {
    if (a >= em.active.size() || b >= em.active.size() || !em.active[a] || !em.active[b] || !em.hasVillageMember[a] ||
        !em.hasVillageMember[b]) {
        return false;
    }

    return em.villageMembers[a].villageId == em.villageMembers[b].villageId;
}

RelationshipEntry* FindRelationship(EntityManager& em, EntityID owner, EntityID other) {
    if (owner >= em.active.size() || !em.active[owner] || !em.hasSocial[owner]) {
        return nullptr;
    }

    for (RelationshipEntry& relationship : em.socials[owner].relationships) {
        if (relationship.otherId == other) {
            return &relationship;
        }
    }

    return nullptr;
}

float GetAggression(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasPersonality[entity]) {
        return 0.0f;
    }

    return em.personalities[entity].aggression;
}

float GetBravery(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasPersonality[entity]) {
        return 0.5f;
    }

    return em.personalities[entity].bravery;
}

float GetPatience(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasPersonality[entity]) {
        return 0.5f;
    }

    return em.personalities[entity].patience;
}

bool HasTrait(const EntityManager& em, EntityID entity, const std::string& traitId) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasPersonality[entity]) {
        return false;
    }

    const std::vector<std::string>& traits = em.personalities[entity].traits;

    return std::find(traits.begin(), traits.end(), traitId) != traits.end();
}

bool HasWitnessNearby(EntityID actor, EntityID target, const EntityManager& em, const EntitySpatialGrid& spatialGrid) {
    if (!IsValidHuman(em, actor) || !IsValidHuman(em, target)) {
        return true;
    }

    const float radius = WITNESS_RADIUS_TILES * Config::TILE_SIZE;

    const std::vector<EntityID> nearby = spatialGrid.GetEntitiesInRadius(em.transforms[target].position, radius, em);

    for (EntityID candidate : nearby) {
        if (candidate == actor || candidate == target) {
            continue;
        }

        if (!IsValidHuman(em, candidate)) {
            continue;
        }

        if (!AreSameVillage(em, actor, candidate)) {
            continue;
        }

        return true;
    }

    return false;
}

bool CanIntimidate(const RelationshipEntry& relationship, const EntityManager& em, EntityID actor) {
    return relationship.resentment >= INTIMIDATE_MIN_RESENTMENT && relationship.friendship <= INTIMIDATE_MAX_FRIENDSHIP &&
           (GetAggression(em, actor) >= 0.30f || GetBravery(em, actor) >= 0.60f);
}

bool CanFightNonLethal(const RelationshipEntry& relationship, const EntityManager& em, EntityID actor) {
    return relationship.resentment >= FIGHT_MIN_RESENTMENT && relationship.friendship <= FIGHT_MAX_FRIENDSHIP &&
           GetAggression(em, actor) >= 0.45f && GetBravery(em, actor) >= 0.45f;
}

bool CanMurder(const RelationshipEntry& relationship, const EntityManager& em, EntityID actor, EntityID target,
               const EntitySpatialGrid& spatialGrid) {
    if (!IsAdultHuman(em, actor) || !IsAdultHuman(em, target)) {
        return false;
    }

    if (relationship.resentment < MURDER_MIN_RESENTMENT || relationship.friendship > MURDER_MAX_FRIENDSHIP) {
        return false;
    }

    const bool hasDarkTrait = HasTrait(em, actor, "VIOLENT") || HasTrait(em, actor, "VENGEFUL");

    if (!hasDarkTrait) {
        return false;
    }

    if (GetAggression(em, actor) < 0.85f) {
        return false;
    }

    if (GetPatience(em, actor) > 0.35f) {
        return false;
    }

    if (HasWitnessNearby(actor, target, em, spatialGrid)) {
        return false;
    }

    // Extremely rare gate.
    // Even with all conditions met, this should almost never happen.
    if (GetRandomValue(1, 1000) != 1) {
        return false;
    }

    return true;
}

EntityID FindBestHostileTarget(EntityID actor, EntityManager& em, const EntitySpatialGrid& spatialGrid,
                               bool (*predicate)(const RelationshipEntry&, const EntityManager&, EntityID)) {
    const float radius = HOSTILITY_RADIUS_TILES * Config::TILE_SIZE;

    const std::vector<EntityID> nearby = spatialGrid.GetEntitiesInRadius(em.transforms[actor].position, radius, em);

    EntityID bestTarget = static_cast<EntityID>(-1);
    float bestScore = -std::numeric_limits<float>::infinity();

    for (EntityID candidate : nearby) {
        if (candidate == actor || !IsValidHuman(em, candidate) || !AreSameVillage(em, actor, candidate)) {
            continue;
        }

        RelationshipEntry* relationship = FindRelationship(em, actor, candidate);

        if (relationship == nullptr) {
            continue;
        }

        if (!predicate(*relationship, em, actor)) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[actor].position, em.transforms[candidate].position);

        const float distancePenalty = std::sqrt(distanceSq) / Config::TILE_SIZE;

        const float score = relationship->resentment + relationship->fear * 0.2f + GetAggression(em, actor) * 30.0f +
                            GetBravery(em, actor) * 15.0f - distancePenalty;

        if (score > bestScore) {
            bestScore = score;
            bestTarget = candidate;
        }
    }

    return bestTarget;
}

EntityID FindBestMurderTarget(EntityID actor, EntityManager& em, const EntitySpatialGrid& spatialGrid) {
    const float radius = HOSTILITY_RADIUS_TILES * Config::TILE_SIZE;

    const std::vector<EntityID> nearby = spatialGrid.GetEntitiesInRadius(em.transforms[actor].position, radius, em);

    EntityID bestTarget = static_cast<EntityID>(-1);
    float bestScore = -std::numeric_limits<float>::infinity();

    for (EntityID candidate : nearby) {
        if (candidate == actor || !IsAdultHuman(em, candidate) || !AreSameVillage(em, actor, candidate)) {
            continue;
        }

        RelationshipEntry* relationship = FindRelationship(em, actor, candidate);

        if (relationship == nullptr) {
            continue;
        }

        if (!CanMurder(*relationship, em, actor, candidate, spatialGrid)) {
            continue;
        }

        const float score = relationship->resentment + GetAggression(em, actor) * 50.0f - relationship->friendship;

        if (score > bestScore) {
            bestScore = score;
            bestTarget = candidate;
        }
    }

    return bestTarget;
}

bool AreAdjacentLocal(EntityID a, EntityID b, const EntityManager& em) {
    if (a >= em.active.size() || b >= em.active.size() || !em.active[a] || !em.active[b] || !em.hasTransform[a] || !em.hasTransform[b]) {
        return false;
    }

    const int ax = AISystemUtils::ToTileCoord(em.transforms[a].position.x);
    const int ay = AISystemUtils::ToTileCoord(em.transforms[a].position.y);
    const int bx = AISystemUtils::ToTileCoord(em.transforms[b].position.x);
    const int by = AISystemUtils::ToTileCoord(em.transforms[b].position.y);

    return std::max(std::abs(ax - bx), std::abs(ay - by)) <= 1;
}

bool StartAdjacentOrMoveToTarget(EntityID actor, EntityID target, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                 const std::string& moveTask, const std::string& actionTask, float actionDuration) {
    if (!IsValidHuman(em, actor) || !IsValidHuman(em, target) || !em.hasBehavior[actor]) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[actor];

    if (AreAdjacentLocal(actor, target, em)) {
        behavior.currentTask = actionTask;
        behavior.currentJobTarget = target;
        behavior.hasJob = true;
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.stateTimer = actionDuration;
        return true;
    }

    std::vector<Vector2> path =
        Pathfinder::FindPathToAdjacentTile(em.transforms[actor].position, em.transforms[target].position, map, tileReg, em, actor);

    if (path.empty()) {
        return false;
    }

    behavior.currentTask = moveTask;
    behavior.currentJobTarget = target;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;
    behavior.stateTimer = 0.0f;

    return true;
}

bool IntimidatePredicate(const RelationshipEntry& relationship, const EntityManager& em, EntityID actor) {
    return CanIntimidate(relationship, em, actor);
}

bool FightPredicate(const RelationshipEntry& relationship, const EntityManager& em, EntityID actor) {
    return CanFightNonLethal(relationship, em, actor);
}

} // namespace

bool AISystem::TryFindIntimidateJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                    const EntitySpatialGrid& spatialGrid) {
    if (!IsValidHuman(em, entity) || !em.hasBehavior[entity]) {
        return false;
    }

    const EntityID target = FindBestHostileTarget(entity, em, spatialGrid, IntimidatePredicate);

    if (target == static_cast<EntityID>(-1)) {
        return false;
    }

    return StartAdjacentOrMoveToTarget(entity, target, em, map, tileReg, "moving_to_intimidate", "intimidating_person", 1.2f);
}

bool AISystem::TryFindFightNonLethalJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                        const EntitySpatialGrid& spatialGrid) {
    if (!IsValidHuman(em, entity) || !em.hasBehavior[entity]) {
        return false;
    }

    const EntityID target = FindBestHostileTarget(entity, em, spatialGrid, FightPredicate);

    if (target == static_cast<EntityID>(-1)) {
        return false;
    }

    return StartAdjacentOrMoveToTarget(entity, target, em, map, tileReg, "moving_to_fight_non_lethal", "fighting_non_lethal", 1.0f);
}

bool AISystem::TryFindMurderJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                const EntitySpatialGrid& spatialGrid) {
    if (!IsAdultHuman(em, entity) || !em.hasBehavior[entity]) {
        return false;
    }

    const EntityID target = FindBestMurderTarget(entity, em, spatialGrid);

    if (target == static_cast<EntityID>(-1)) {
        return false;
    }

    return StartAdjacentOrMoveToTarget(entity, target, em, map, tileReg, "moving_to_murder", "murdering_person", 1.5f);
}
