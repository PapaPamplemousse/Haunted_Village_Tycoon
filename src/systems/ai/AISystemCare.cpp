/**
 * @file AISystemCare.cpp
 * @brief AI logic for lightweight family care tasks.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/AISystem.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/Pathfinder.hpp"

#include <limits>
#include <utility>
#include <vector>

namespace {

constexpr float CHILD_HUNGER_CARE_THRESHOLD_RATIO = 0.5f;

bool IsValidHungryChildOf(EntityID parent, EntityID child, const EntityManager& em) {
    if (parent >= em.active.size() || child >= em.active.size() || !em.active[parent] || !em.active[child] || !em.hasFamily[child] ||
        !em.hasNeeds[child] || !em.hasTransform[child]) {
        return false;
    }

    const FamilyComponent& childFamily = em.families[child];

    const bool isChildOfParent = childFamily.parentA == parent || childFamily.parentB == parent;

    if (!isChildOfParent) {
        return false;
    }

    const NeedsComponent& needs = em.needs[child];

    if (needs.maxHunger <= 0.0f) {
        return false;
    }

    const float hungerRatio = needs.hunger / needs.maxHunger;

    return hungerRatio < CHILD_HUNGER_CARE_THRESHOLD_RATIO;
}

} // namespace

bool AISystem::TryFindCareChildFoodJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                       const ResourceRegistry* resourceReg) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasFamily[entity] || !em.hasInventory[entity] || !em.hasBehavior[entity] ||
        !em.hasTransform[entity]) {
        return false;
    }

    const std::string foodItem = AISystemUtils::FindFirstFoodItemInInventory(em.inventories[entity], resourceReg);

    if (foodItem.empty()) {
        return false;
    }

    const FamilyComponent& family = em.families[entity];

    EntityID bestChild = static_cast<EntityID>(-1);
    float bestScore = -std::numeric_limits<float>::infinity();

    for (EntityID child : family.children) {
        if (!IsValidHungryChildOf(entity, child, em)) {
            continue;
        }

        const NeedsComponent& childNeeds = em.needs[child];

        const float hungerRatio = childNeeds.maxHunger > 0.0f ? childNeeds.hunger / childNeeds.maxHunger : 1.0f;

        const float urgencyScore = (1.0f - hungerRatio) * 100.0f;

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[child].position);

        const float distancePenalty = distanceSq / 10000.0f;

        const float score = urgencyScore - distancePenalty;

        if (score > bestScore) {
            bestScore = score;
            bestChild = child;
        }
    }

    if (bestChild == static_cast<EntityID>(-1)) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[entity];

    if (em.hasAIContext[entity]) {
        em.aiContexts[entity].careTargetId = bestChild;
    }

    if (AreEntitiesAdjacent(entity, bestChild, em)) {
        behavior.currentTask = "feeding_child";
        behavior.currentJobTarget = bestChild;
        behavior.currentItemTarget = foodItem;
        behavior.hasJob = true;
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.stateTimer = AISystemUtils::EAT_DURATION;

        return true;
    }

    std::vector<Vector2> path =
        Pathfinder::FindPathToAdjacentTile(em.transforms[entity].position, em.transforms[bestChild].position, map, tileReg, em, entity);

    if (path.empty()) {
        return false;
    }

    behavior.currentTask = "moving_to_feed_child";
    behavior.currentJobTarget = bestChild;
    behavior.currentItemTarget = foodItem;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;

    return true;
}
