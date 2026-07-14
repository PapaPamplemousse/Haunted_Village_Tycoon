/**
 * @file AIRepairCompletion.cpp
 * @brief Completion handlers for repair tasks.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/ai/AICompletion.hpp"

#include <algorithm>

namespace ai::completion {

AICompletionStatus TryCompleteRepairTask(const AICompletionContext& ctx) {
    EntityManager& em = ctx.em;
    BehaviorComponent& behavior = ctx.behavior();

    if (behavior.currentTask != "repairing") {
        return AICompletionStatus::NotHandled;
    }

    const EntityID target = behavior.currentJobTarget;

    if (target < em.active.size() && em.active[target] && em.hasHealth[target]) {
        em.healths[target].current = std::min(em.healths[target].max, em.healths[target].current + 25.0f);

        if (em.healths[target].current < em.healths[target].max) {
            behavior.stateTimer = 1.0f;
            return AICompletionStatus::InProgress;
        }
    }

    return AICompletionStatus::Completed;
}

} // namespace ai::completion
