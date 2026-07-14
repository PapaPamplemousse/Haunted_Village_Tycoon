/**
 * @file AICraftingRequestIntents.cpp
 * @brief Crafting/request fulfilment AI intents.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/AISystemUtils.hpp"
#include "systems/VillageRequestSystem.hpp"
#include "systems/ai/AIIntentFinders.hpp"

#include <string>

namespace {

bool IsValidActor(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasTransform[entity] && em.hasBehavior[entity] && em.hasAIContext[entity];
}

bool IsBlacksmith(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasProfession[entity] &&
           em.professions[entity].currentProfession == "blacksmith";
}

EntityID FindAssignedWeaponRequest(EntityID worker, const EntityManager& em, std::string& outItemId) {
    outItemId.clear();

    for (EntityID request = 0; request < em.active.size(); ++request) {
        if (!em.active[request] || !em.hasVillageRequest[request]) {
            continue;
        }

        const VillageRequestComponent& req = em.villageRequests[request];

        if (req.type != VillageRequestType::WeaponNeeded) {
            continue;
        }

        if (req.assigneeId != worker) {
            continue;
        }

        if (req.requestedItemId.empty()) {
            continue;
        }

        outItemId = req.requestedItemId;
        return request;
    }

    return static_cast<EntityID>(-1);
}

} // namespace

namespace ai::intents {

std::optional<AIIntent> FindFulfillWeaponRequestIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&,
                                                       const ResourceRegistry&, const WeaponRegistry&, const EntitySpatialGrid&) {
    if (!IsValidActor(em, entity) || !IsBlacksmith(em, entity)) {
        return std::nullopt;
    }

    std::string itemId;
    const EntityID requestId = FindAssignedWeaponRequest(entity, em, itemId);

    if (requestId == static_cast<EntityID>(-1) || itemId.empty()) {
        return std::nullopt;
    }

    AIContextComponent& context = em.aiContexts[entity];
    context.activeRequestId = requestId;
    context.requestedCraftItemId = itemId;

    AIIntent intent;
    intent.kind = AIIntentKind::StartAction;
    intent.targetEntity = requestId;
    intent.actionTask = "crafting_weapon";
    intent.actionDuration = 3.0f;
    intent.itemTarget = itemId;

    return intent;
}

} // namespace ai::intents
