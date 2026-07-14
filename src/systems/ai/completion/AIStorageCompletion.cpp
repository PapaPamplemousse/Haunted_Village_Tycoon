/**
 * @brief Completion handlers for AI storage and hauling tasks.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/AISystemUtils.hpp"
#include "systems/ai/AICompletion.hpp"

#include <algorithm>

namespace ai::completion {

AICompletionStatus TryCompleteStorageTask(const AICompletionContext& ctx) {
    EntityManager& em = ctx.em;
    const EntityID entity = ctx.entity;
    BehaviorComponent& behavior = ctx.behavior();

    if (behavior.currentTask == "depositing") {
        const EntityID storage = behavior.currentJobTarget;

        if (storage < em.active.size() && em.active[storage] && em.hasInventory[entity] && em.hasInventory[storage] &&
            em.hasStorage[storage]) {
            AISystemUtils::DepositInventoryIntoStorage(em.inventories[entity], em.inventories[storage], em.storages[storage]);
        }

        return AICompletionStatus::Completed;
    }

    if (behavior.currentTask == "hauling_deposit") {
        if (!em.hasAIContext[entity] || !em.hasInventory[entity]) {
            return AICompletionStatus::Completed;
        }

        AIContextComponent& context = em.aiContexts[entity];

        const EntityID destination = context.haulDestinationId;
        const std::string itemId = context.haulItemId;

        if (destination < em.active.size() && em.active[destination] && em.hasInventory[destination] && em.hasStorage[destination] &&
            !itemId.empty()) {
            const int remainingCapacity =
                em.storages[destination].capacity - AISystemUtils::GetInventoryItemCount(em.inventories[destination]);

            if (remainingCapacity > 0) {
                const int amount =
                    AISystemUtils::RemoveItemFromInventory(em.inventories[entity], itemId, std::min(context.haulAmount, remainingCapacity));

                if (amount > 0) {
                    em.inventories[destination].items[itemId] += amount;
                }
            }
        }

        context.haulSourceId = static_cast<EntityID>(-1);
        context.haulDestinationId = static_cast<EntityID>(-1);
        context.haulItemId.clear();
        context.haulAmount = 0;

        return AICompletionStatus::Completed;
    }

    return AICompletionStatus::NotHandled;
}

} // namespace ai::completion
