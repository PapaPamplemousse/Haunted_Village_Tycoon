/**
 * @file AIRequestCompletion.cpp
 * @brief Completion handlers for village request tasks.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/VillageRequestSystem.hpp"
#include "systems/ai/AICompletion.hpp"

namespace ai::completion {

AICompletionStatus TryCompleteRequestTask(const AICompletionContext& ctx) {
    EntityManager& em = ctx.em;
    const EntityID entity = ctx.entity;
    BehaviorComponent& behavior = ctx.behavior();

    if (behavior.currentTask != "requesting_weapon") {
        return AICompletionStatus::NotHandled;
    }

    const EntityID blacksmith = behavior.currentJobTarget;
    const std::string requestedWeapon = behavior.currentItemTarget.empty() ? "SPEAR" : behavior.currentItemTarget;

    if (blacksmith < em.active.size() && em.active[blacksmith] && em.hasVillageMember[entity]) {
        const EntityID villageId = em.villageMembers[entity].villageId;

        const EntityID requestId =
            VillageRequestSystem::CreateRequest(em, VillageRequestType::WeaponNeeded, entity, villageId, requestedWeapon, 1, 750.0f);

        if (requestId != static_cast<EntityID>(-1)) {
            VillageRequestSystem::AssignRequest(em, requestId, blacksmith);
        }
    }

    return AICompletionStatus::Completed;
}

} // namespace ai::completion
