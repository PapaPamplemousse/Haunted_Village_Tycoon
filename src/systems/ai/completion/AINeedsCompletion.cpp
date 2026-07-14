/**
 * @file AINeedsCompletion.cpp
 * @brief Completion handlers for AI need-related tasks.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/ai/AICompletion.hpp"

namespace ai::completion {

AICompletionStatus TryCompleteNeedsTask(const AICompletionContext& ctx) {
    EntityManager& em = ctx.em;
    const EntityID entity = ctx.entity;
    BehaviorComponent& behavior = ctx.behavior();

    if (behavior.currentTask == "resting") {
        if (em.hasNeeds[entity]) {
            NeedsComponent& needs = em.needs[entity];

            needs.fatigue -= Config::FATIGUE_REST_RECOVERY_PER_SECOND;

            if (needs.fatigue < 0.0f) {
                needs.fatigue = 0.0f;
            }

            if (!AISystemUtils::IsFullyRested(entity, em)) {
                behavior.stateTimer = 1.0f;
                return AICompletionStatus::Completed;
            }
        }

        AISystemUtils::ReleaseRestSpotReservation(entity, em);
        return AICompletionStatus::Completed;
    }

    if (behavior.currentTask == "eating") {
        if (em.hasNeeds[entity] && em.hasInventory[entity] && !behavior.currentItemTarget.empty()) {
            AISystemUtils::ConsumeFoodFromInventory(em.inventories[entity], em.needs[entity], behavior.currentItemTarget, ctx.resourceReg);
        }

        return AICompletionStatus::Completed;
    }

    if (behavior.currentTask == "eating_from_storage") {
        const EntityID storage = behavior.currentJobTarget;

        if (storage < em.active.size() && em.active[storage] && em.hasInventory[storage] && em.hasNeeds[entity] &&
            !behavior.currentItemTarget.empty()) {
            AISystemUtils::ConsumeFoodFromInventory(em.inventories[storage], em.needs[entity], behavior.currentItemTarget, ctx.resourceReg);
        }

        return AICompletionStatus::Completed;
    }

    if (behavior.currentTask == "feeding_child") {
        const EntityID child = behavior.currentJobTarget;

        if (child < em.active.size() && em.active[child] && em.hasNeeds[child] && em.hasInventory[entity] &&
            !behavior.currentItemTarget.empty()) {
            AISystemUtils::ConsumeFoodFromInventory(em.inventories[entity], em.needs[child], behavior.currentItemTarget, ctx.resourceReg);
        }

        if (em.hasAIContext[entity]) {
            em.aiContexts[entity].careTargetId = static_cast<EntityID>(-1);
        }

        return AICompletionStatus::Completed;
    }

    return AICompletionStatus::NotHandled;
}

} // namespace ai::completion
