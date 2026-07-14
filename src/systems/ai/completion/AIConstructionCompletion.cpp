/**
 * @file AIConstructionCompletion.cpp
 * @brief Completion handlers for building and dismantling tasks.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/AISystemUtils.hpp"
#include "systems/ai/AICompletion.hpp"

#include <algorithm>

namespace ai::completion {

AICompletionStatus TryCompleteConstructionTask(const AICompletionContext& ctx) {
    EntityManager& em = ctx.em;
    const EntityID entity = ctx.entity;
    BehaviorComponent& behavior = ctx.behavior();

    if (behavior.currentTask == "building") {
        const EntityID target = behavior.currentJobTarget;

        if (target < em.active.size() && em.active[target] && em.hasBlueprint[target]) {
            if (AISystemUtils::ConsumeAccessibleMaterials(entity, em, ctx.spatialGrid, em.blueprints[target].requiredMaterials)) {
                em.blueprints[target].isFinished = true;
                em.hasBlueprint[target] = false;

                if (em.hasConstruction[target] || (em.hasTag[target] && !em.hasBehavior[target])) {
                    ctx.roomSys.MarkDirty();
                }
            }

            if (em.hasConstruction[target] || (em.hasTag[target] && !em.hasBehavior[target])) {
                ctx.roomSys.MarkDirty();
            }
        }

        return AICompletionStatus::Completed;
    }

    if (behavior.currentTask == "dismantling") {
        const EntityID target = behavior.currentJobTarget;

        if (target < em.active.size() && em.active[target] && em.hasDeconstruct[target]) {
            if (em.hasCost[target] && em.hasInventory[entity]) {
                for (const auto& req : em.costs[target].materials) {
                    const int refund = std::max(1, req.second / 2);
                    em.inventories[entity].items[req.first] += refund;
                }
            }

            if (em.hasConstruction[target] || (em.hasTag[target] && !em.hasBehavior[target])) {
                ctx.roomSys.MarkDirty();
            }

            em.DestroyEntity(target);
        }

        return AICompletionStatus::Completed;
    }

    return AICompletionStatus::NotHandled;
}

} // namespace ai::completion
