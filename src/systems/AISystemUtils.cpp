#include "systems/AISystemUtils.hpp"

#include "core/Config.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace AISystemUtils {

int ToTileCoord(float worldCoord) {
    return static_cast<int>(std::floor(worldCoord / Config::TILE_SIZE));
}

float SquaredDistance(Vector2 a, Vector2 b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;

    return dx * dx + dy * dy;
}

bool IsWithinActiveSimulationRadius(Vector2 entityPosition, Vector2 simulationCenter, float activeRadiusTiles) {
    const float radiusWorld = activeRadiusTiles * Config::TILE_SIZE;
    const float radiusSq = radiusWorld * radiusWorld;

    const float dx = entityPosition.x - simulationCenter.x;
    const float dy = entityPosition.y - simulationCenter.y;

    return dx * dx + dy * dy <= radiusSq;
}

float GetActionRadiusWorld(EntityID entity, const EntityManager& em) {
    if (entity >= em.active.size() || !em.hasStats[entity]) {
        return static_cast<float>(Config::AI_SEARCH_RADIUS_TILES) * Config::TILE_SIZE;
    }

    return em.stats[entity].actionRadiusTiles * Config::TILE_SIZE;
}

void GiveLootToInventory(EntityID receiver, EntityID source, EntityManager& em) {
    if (receiver >= em.active.size() || source >= em.active.size() || !em.active[receiver] || !em.hasInventory[receiver] ||
        !em.hasLoot[source]) {
        return;
    }

    for (const DropEntry& drop : em.loots[source].drops) {
        const float roll = static_cast<float>(GetRandomValue(0, 100)) / 100.0f;

        if (roll <= drop.chance) {
            em.inventories[receiver].items[drop.itemId] += drop.amount;
        }
    }
}

bool IsConsumableFoodItem(const ResourceRegistry& resourceReg, const std::string& itemId) {
    const ResourceDef* resource = resourceReg.GetResourceDef(itemId);

    if (resource == nullptr) {
        return false;
    }

    return resource->isConsumable && resource->nutrition > 0.0f;
}

std::string FindFirstFoodItemInInventory(const InventoryComponent& inventory, const ResourceRegistry& resourceReg) {
    for (const auto& item : inventory.items) {
        const std::string& itemId = item.first;
        const int count = item.second;

        if (count <= 0) {
            continue;
        }

        if (IsConsumableFoodItem(resourceReg, itemId)) {
            return itemId;
        }
    }

    return "";
}

bool ConsumeFoodFromInventory(InventoryComponent& inventory, NeedsComponent& needs, const std::string& itemId,
                              const ResourceRegistry& resourceReg) {
    auto it = inventory.items.find(itemId);

    if (it == inventory.items.end() || it->second <= 0) {
        return false;
    }

    const ResourceDef* resource = resourceReg.GetResourceDef(itemId);

    if (resource == nullptr || !resource->isConsumable || resource->nutrition <= 0.0f) {
        return false;
    }

    it->second--;

    if (it->second <= 0) {
        inventory.items.erase(it);
    }

    needs.hunger += resource->nutrition;

    if (needs.hunger > needs.maxHunger) {
        needs.hunger = needs.maxHunger;
    }

    return true;
}

bool HarvestableHasFoodDrop(const HarvestableComponent& harvestable, const ResourceRegistry& resourceReg) {
    for (const DropEntry& drop : harvestable.drops) {
        if (drop.amount <= 0 || drop.itemId.empty()) {
            continue;
        }

        if (IsConsumableFoodItem(resourceReg, drop.itemId)) {
            return true;
        }
    }

    return false;
}

bool HasRequiredHarvestTool(EntityID worker, const HarvestableComponent& harvestable, const EntityManager& em) {
    if (harvestable.requiredTool == "none") {
        return true;
    }

    if (worker >= em.active.size() || !em.hasEquipment[worker]) {
        return false;
    }

    return em.equipments[worker].rightHandToolType == harvestable.requiredTool;
}

int GetInventoryItemCount(const InventoryComponent& inventory) {
    int total = 0;

    for (const auto& item : inventory.items) {
        if (item.second > 0) {
            total += item.second;
        }
    }

    return total;
}

bool HasAnyInventoryItem(const InventoryComponent& inventory) {
    for (const auto& item : inventory.items) {
        if (item.second > 0) {
            return true;
        }
    }

    return false;
}

bool StorageAcceptsItem(const StorageComponent& storage, const std::string& itemId) {
    if (storage.acceptedItems.empty()) {
        return true;
    }

    return std::find(storage.acceptedItems.begin(), storage.acceptedItems.end(), itemId) != storage.acceptedItems.end();
}

bool HasAvailableStorageCapacity(EntityID storageEntity, const EntityManager& em) {
    if (storageEntity >= em.active.size() || !em.active[storageEntity] || !em.hasStorage[storageEntity] ||
        !em.hasInventory[storageEntity]) {
        return false;
    }

    const int usedCapacity = GetInventoryItemCount(em.inventories[storageEntity]);
    return usedCapacity < em.storages[storageEntity].capacity;
}

bool StorageCanAcceptFromInventory(EntityID storageEntity, const InventoryComponent& sourceInventory, const EntityManager& em) {
    if (storageEntity >= em.active.size() || !em.active[storageEntity] || !em.hasStorage[storageEntity] ||
        !em.hasInventory[storageEntity]) {
        return false;
    }

    if (!HasAvailableStorageCapacity(storageEntity, em)) {
        return false;
    }

    const StorageComponent& storage = em.storages[storageEntity];

    for (const auto& item : sourceInventory.items) {
        if (item.second <= 0) {
            continue;
        }

        if (StorageAcceptsItem(storage, item.first)) {
            return true;
        }
    }

    return false;
}

void DepositInventoryIntoStorage(InventoryComponent& sourceInventory, InventoryComponent& storageInventory,
                                 const StorageComponent& storage) {
    int usedCapacity = GetInventoryItemCount(storageInventory);
    int remainingCapacity = storage.capacity - usedCapacity;

    if (remainingCapacity <= 0) {
        return;
    }

    std::vector<std::string> emptyItems;

    for (auto& item : sourceInventory.items) {
        if (remainingCapacity <= 0) {
            break;
        }

        const std::string& itemId = item.first;
        int& sourceCount = item.second;

        if (sourceCount <= 0) {
            emptyItems.push_back(itemId);
            continue;
        }

        if (!StorageAcceptsItem(storage, itemId)) {
            continue;
        }

        const int movedAmount = std::min(sourceCount, remainingCapacity);

        storageInventory.items[itemId] += movedAmount;
        sourceCount -= movedAmount;
        remainingCapacity -= movedAmount;

        if (sourceCount <= 0) {
            emptyItems.push_back(itemId);
        }
    }

    for (const std::string& itemId : emptyItems) {
        sourceInventory.items.erase(itemId);
    }
}

bool IsHourInRange(float hour, float startHour, float endHour) {
    if (startHour == endHour) {
        return true;
    }

    if (startHour < endHour) {
        return hour >= startHour && hour < endHour;
    }

    return hour >= startHour || hour < endHour;
}

bool CanStartWorkNow(const BehaviorComponent& behavior, float currentHour) {
    if (behavior.activityPeriod == "any") {
        return true;
    }

    return IsHourInRange(currentHour, behavior.workStartHour, behavior.workEndHour);
}

bool ShouldDepositInventory(EntityID entity, const EntityManager& em, const BehaviorComponent& behavior, float currentHour) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasInventory[entity]) {
        return false;
    }

    const InventoryComponent& inventory = em.inventories[entity];

    if (!HasAnyInventoryItem(inventory)) {
        return false;
    }

    const int itemCount = GetInventoryItemCount(inventory);

    if (itemCount >= behavior.storeThreshold) {
        return true;
    }

    if (!CanStartWorkNow(behavior, currentHour)) {
        return true;
    }

    return false;
}

} // namespace AISystemUtils
