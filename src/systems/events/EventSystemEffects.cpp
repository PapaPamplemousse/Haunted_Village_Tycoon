/**
 * @file EventSystemEffects.cpp
 * @brief Implements direct settlement impacts (adding fear, corruption, stealing food).
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/EventSystem.hpp"

#include <algorithm>

void EventSystem::AddFear(SettlementMetrics& metrics, float amount) {
    metrics.fear = std::clamp(metrics.fear + amount, 0.0f, 100.0f);
}

void EventSystem::AddCorruption(SettlementMetrics& metrics, float amount) {
    metrics.corruption = std::clamp(metrics.corruption + amount, 0.0f, 100.0f);
}

bool EventSystem::IsFoodItem(const ResourceRegistry& resourceReg, const std::string& itemId) const {
    const ResourceDef* resource = resourceReg.GetResourceDef(itemId);

    if (resource == nullptr) {
        return false;
    }

    return resource->isConsumable && resource->nutrition > 0.0f;
}

bool EventSystem::RemoveFoodFromAnyStorage(EntityManager& em, const ResourceRegistry& resourceReg, int amountToRemove,
                                           std::string& outItemId, int& outRemovedAmount) {
    outItemId.clear();
    outRemovedAmount = 0;

    if (amountToRemove <= 0) {
        return false;
    }

    for (size_t entity = 0; entity < em.active.size(); ++entity) {
        if (!em.active[entity] || !em.hasInventory[entity] || !em.hasStorage[entity]) {
            continue;
        }

        if (em.hasBlueprint[entity] && !em.blueprints[entity].isFinished) {
            continue;
        }

        auto& inventory = em.inventories[entity];

        for (auto it = inventory.items.begin(); it != inventory.items.end(); ++it) {
            const std::string& itemId = it->first;
            int& count = it->second;

            if (count <= 0) {
                continue;
            }

            if (!IsFoodItem(resourceReg, itemId)) {
                continue;
            }

            const int removed = std::min(count, amountToRemove);

            count -= removed;
            outRemovedAmount = removed;
            outItemId = itemId;

            if (count <= 0) {
                inventory.items.erase(it);
            }

            return removed > 0;
        }
    }

    return false;
}
