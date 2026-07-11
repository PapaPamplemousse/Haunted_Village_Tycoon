#include "systems/AISystem.hpp"

#include "core/Config.hpp"
#include "systems/Pathfinder.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <raymath.h>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr float ATTACK_DURATION = 0.45f;
constexpr float ATTACK_COOLDOWN = 0.6f;

constexpr float SEEK_FOOD_THRESHOLD_RATIO = 0.5f;
constexpr float EAT_DURATION = 1.0f;

int ToTileCoord(float worldCoord) {
    return static_cast<int>(std::floor(worldCoord / Config::TILE_SIZE));
}

float SquaredDistance(Vector2 a, Vector2 b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

void GiveLootToInventory(EntityID receiver, EntityID source, EntityManager& em) {
    if (receiver >= em.active.size() || source >= em.active.size() || !em.active[receiver] || !em.hasInventory[receiver] ||
        !em.hasLoot[source]) {
        return;
    }

    for (const DropEntry& drop : em.loots[source].drops) {
        float roll = static_cast<float>(GetRandomValue(0, 100)) / 100.0f;

        if (roll <= drop.chance) {
            em.inventories[receiver].items[drop.itemId] += drop.amount;
        }
    }
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

    if (!em.hasEquipment[worker]) {
        return false;
    }

    return em.equipments[worker].rightHandToolType == harvestable.requiredTool;
}

bool StorageAcceptsItem(const StorageComponent& storage, const std::string& itemId) {
    if (storage.acceptedItems.empty()) {
        return true;
    }

    return std::find(storage.acceptedItems.begin(), storage.acceptedItems.end(), itemId) != storage.acceptedItems.end();
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

bool HasAnyItemAcceptedByStorage(const InventoryComponent& sourceInventory, const StorageComponent& storage) {
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

    return HasAnyItemAcceptedByStorage(sourceInventory, em.storages[storageEntity]);
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

    // Range wraps around midnight.
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

    // End of work day: deposit even below threshold.
    if (!CanStartWorkNow(behavior, currentHour)) {
        return true;
    }

    return false;
}

} // namespace

void AISystem::Update(float deltaTime, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                      const ResourceRegistry& resourceReg, const EntitySpatialGrid& spatialGrid, const Vector2& simulationCenter,
                      float activeRadiusTiles, float currentHour, RoomSystem& roomSys) {
    for (size_t i = 0; i < em.active.size(); ++i) {
        if (!em.active[i]) {
            continue;
        }

        if (!em.hasBehavior[i] || !em.hasTransform[i] || !em.hasStats[i]) {
            continue;
        }

        if (!IsWithinActiveSimulationRadius(em.transforms[i].position, simulationCenter, activeRadiusTiles)) {
            continue;
        }

        auto& behavior = em.behaviors[i];

        if (behavior.stateTimer > 0.0f) {
            behavior.stateTimer -= deltaTime;
            continue;
        }

        if (behavior.currentTask == "idle") {
            HandleIdleState(i, em, map, tileReg, resourceReg, spatialGrid, currentHour);
        } else if (behavior.isMoving) {
            HandleMovingState(i, deltaTime, em);
        } else {
            HandleTaskCompletion(i, em, resourceReg, roomSys);
        }
    }
}

// ============================================================================
// STATE HANDLERS
// ============================================================================

void AISystem::HandleIdleState(EntityID i, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                               const ResourceRegistry& resourceReg, const EntitySpatialGrid& spatialGrid, float currentHour) {
    auto& behavior = em.behaviors[i];

    const bool canHunt = HasCapability(behavior, "hunt");
    const bool canHarvest = HasCapability(behavior, "harvest");
    const bool canBuild = HasCapability(behavior, "build");
    const bool canDismantle = HasCapability(behavior, "dismantle");
    const bool canWander = HasCapability(behavior, "wander");
    const bool canSeekFood = HasCapability(behavior, "seek_food");
    const bool canStore = HasCapability(behavior, "store");

    const bool canStartWork = CanStartWorkNow(behavior, currentHour);

    // Survival behavior is always allowed.
    if (canSeekFood && TryFindSeekFoodJob(i, em, map, tileReg, resourceReg, spatialGrid)) {
        return;
    }

    // Deposit if threshold is reached or if the work day is over.
    if (canStore && ShouldDepositInventory(i, em, behavior, currentHour)) {
        if (TryFindStoreJob(i, em, map, tileReg, spatialGrid)) {
            return;
        }
    }

    // Outside work hours:
    // - do not start new productive jobs.
    // - only wander/idle for now. Fatigue/rest will come later.
    if (!canStartWork) {
        if (canWander && TryFindWanderJob(i, em, map, tileReg)) {
            return;
        }

        behavior.stateTimer = 1.0f;
        return;
    }

    if (canHunt && TryFindHuntJob(i, em, map, tileReg, spatialGrid)) {
        return;
    }

    if (canBuild && TryFindBuildJob(i, em, map, tileReg, spatialGrid)) {
        return;
    }

    if (canDismantle && TryFindDismantleJob(i, em, map, tileReg, spatialGrid)) {
        return;
    }

    if (canHarvest && TryFindHarvestJob(i, em, map, tileReg, spatialGrid)) {
        return;
    }

    if (canWander && TryFindWanderJob(i, em, map, tileReg)) {
        return;
    }
}

void AISystem::InteractWithDoorIfPresent(EntityID entity, int targetX, int targetY, EntityManager& em) {
    for (size_t i = 0; i < em.active.size(); ++i) {
        if (!em.active[i] || !em.hasDoor[i] || !em.hasTransform[i]) {
            continue;
        }

        const int doorX = ToTileCoord(em.transforms[i].position.x);
        const int doorY = ToTileCoord(em.transforms[i].position.y);

        if (doorX != targetX || doorY != targetY) {
            continue;
        }

        DoorComponent& door = em.doors[i];

        if (door.state == DoorState::LOCKED && door.ownerId != entity) {
            return;
        }

        if (door.state == DoorState::CLOSED) {
            door.state = DoorState::OPEN;
        }

        return;
    }
}

void AISystem::HandleMovingState(EntityID i, float deltaTime, EntityManager& em) {
    auto& behavior = em.behaviors[i];
    auto& transform = em.transforms[i];

    const float speed = em.stats[i].maxSpeed;

    if (behavior.currentPath.empty() || behavior.currentPathIndex >= behavior.currentPath.size()) {
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        return;
    }

    behavior.currentTarget = behavior.currentPath[behavior.currentPathIndex];

    const int currentTileX = ToTileCoord(transform.position.x);
    const int currentTileY = ToTileCoord(transform.position.y);

    const int targetTileX = ToTileCoord(behavior.currentTarget.x);
    const int targetTileY = ToTileCoord(behavior.currentTarget.y);

    const int tileDx = std::abs(targetTileX - currentTileX);
    const int tileDy = std::abs(targetTileY - currentTileY);

    if (std::max(tileDx, tileDy) <= 1) {
        InteractWithDoorIfPresent(i, targetTileX, targetTileY, em);
    }

    Vector2 dir = Vector2Subtract(behavior.currentTarget, transform.position);
    const float distanceToTarget = Vector2Length(dir);
    const float step = speed * deltaTime;

    if (distanceToTarget <= step || distanceToTarget < 1.0f) {
        transform.position = behavior.currentTarget;

        behavior.currentPathIndex++;

        if (behavior.currentPathIndex < behavior.currentPath.size()) {
            behavior.currentTarget = behavior.currentPath[behavior.currentPathIndex];
            return;
        }

        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;

        if (behavior.currentTask == "moving_to_build") {
            behavior.currentTask = "building";
            behavior.stateTimer = 2.0f;
        } else if (behavior.currentTask == "moving_to_dismantle") {
            behavior.currentTask = "dismantling";
            behavior.stateTimer = 2.0f;
        } else if (behavior.currentTask == "moving_to_storage") {
            behavior.currentTask = "depositing";
            behavior.stateTimer = 0.8f;
        } else if (behavior.currentTask == "moving_to_food_storage") {
            behavior.currentTask = "eating_from_storage";
            behavior.stateTimer = EAT_DURATION;
        } else if (behavior.currentTask == "moving_to_harvest") {
            behavior.currentTask = "harvesting";
            behavior.stateTimer = 1.0f;
            behavior.actionAccumulator = 0.0f;
        } else if (behavior.currentTask == "moving_to_hunt") {
            behavior.currentTask = "attacking";
            behavior.stateTimer = ATTACK_DURATION;
        } else if (behavior.currentTask == "wandering") {
            behavior.currentTask = "idle";
            behavior.stateTimer = GetRandomValue(10, 40) / 10.0f;
        }

        return;
    }

    if (distanceToTarget <= 0.0f) {
        return;
    }

    Vector2 normalizedDir = Vector2Scale(dir, 1.0f / distanceToTarget);

    transform.position.x += normalizedDir.x * step;
    transform.position.y += normalizedDir.y * step;
}

void AISystem::HandleTaskCompletion(EntityID i, EntityManager& em, const ResourceRegistry& resourceReg, RoomSystem& roomSys) {
    auto& behavior = em.behaviors[i];

    if (behavior.currentTask == "building") {
        EntityID target = behavior.currentJobTarget;

        if (target < em.active.size() && em.active[target] && em.hasBlueprint[target]) {
            if (em.hasInventory[i]) {
                for (const auto& req : em.blueprints[target].requiredMaterials) {
                    em.inventories[i].items[req.first] -= req.second;
                }
            }

            em.blueprints[target].isFinished = true;
            em.hasBlueprint[target] = false;

            if (em.hasConstruction[target] || (em.hasTag[target] && !em.hasBehavior[target])) {
                roomSys.MarkDirty();
            }
        }
    } else if (behavior.currentTask == "dismantling") {
        EntityID target = behavior.currentJobTarget;

        if (target < em.active.size() && em.active[target] && em.hasDeconstruct[target]) {
            if (em.hasCost[target] && em.hasInventory[i]) {
                for (const auto& req : em.costs[target].materials) {
                    int refund = std::max(1, req.second / 2);
                    em.inventories[i].items[req.first] += refund;
                }
            }

            if (em.hasConstruction[target] || (em.hasTag[target] && !em.hasBehavior[target])) {
                roomSys.MarkDirty();
            }

            em.DestroyEntity(target);
        }
    } else if (behavior.currentTask == "harvesting") {
        EntityID target = behavior.currentJobTarget;

        // On vérifie que la cible existe, qu'elle est récoltable, et qu'elle a des PV
        if (target < em.active.size() && em.active[target] && em.hasHarvestable[target] && em.hasHealth[target]) {
            const auto& harvestable = em.harvestables[target];

            // 1. Calcul des dégâts (Stats de base + Arme)
            float damage = em.stats[i].baseAttack;
            std::string toolType = "none";

            if (em.hasEquipment[i]) {
                damage += em.equipments[i].rightHandDamage;
                toolType = em.equipments[i].rightHandToolType;
            }

            // 2. Application des dégâts
            em.healths[target].current -= damage;
            behavior.actionAccumulator += 1.0f; // +1 seconde de passée

            // 3. Récupération des ressources (Toutes les 3 secondes)
            if (behavior.actionAccumulator >= 3.0f) {
                behavior.actionAccumulator = 0.0f; // Reset du timer de loot

                // Vérification de l'outil requis
                if (harvestable.requiredTool == "none" || harvestable.requiredTool == toolType) {
                    if (em.hasInventory[i]) {
                        for (const DropEntry& drop : harvestable.drops) {
                            float roll = static_cast<float>(GetRandomValue(0, 100)) / 100.0f;

                            if (roll <= drop.chance) {
                                em.inventories[i].items[drop.itemId] += drop.amount;
                            }
                        }
                    }
                }
            }

            // 4. L'arbre est-il détruit ?
            if (em.healths[target].current <= 0.0f) {
                if (em.hasConstruction[target]) {
                    roomSys.MarkDirty();
                }
                em.DestroyEntity(target);
                // L'arbre est mort. Le code va descendre naturellement et atteindre
                // le ResetBehaviorState(behavior); global situé à la fin de la fonction !
            } else {
                // 5. L'arbre est encore en vie ! On boucle.
                behavior.stateTimer = 1.0f; // Prochain coup de hache dans 1 seconde
                return;                     // TRÈS IMPORTANT : On sort pour NE PAS appeler le ResetBehaviorState() global !
            }
        }
    } else if (behavior.currentTask == "depositing") {
        EntityID storage = behavior.currentJobTarget;

        if (storage < em.active.size() && em.active[storage] && em.hasInventory[i] && em.hasInventory[storage] && em.hasStorage[storage]) {
            // Do not deposit into unfinished blueprints.
            if (!(em.hasBlueprint[storage] && !em.blueprints[storage].isFinished)) {
                DepositInventoryIntoStorage(em.inventories[i], em.inventories[storage], em.storages[storage]);
            }
        }
    } else if (behavior.currentTask == "attacking") {
        EntityID target = behavior.currentJobTarget;

        bool attackSucceeded = false;

        if (target < em.active.size() && em.active[target] && em.hasHealth[target] && em.hasTransform[target] &&
            AreEntitiesAdjacent(i, target, em)) {
            float damage = 0.0f;

            if (em.hasStats[i]) {
                damage += em.stats[i].baseAttack;
            }

            if (em.hasEquipment[i]) {
                damage += em.equipments[i].rightHandDamage;
            }

            // Safety fallback: avoid zero-damage attacks if an entity has no stats/equipment.
            if (damage <= 0.0f) {
                damage = 1.0f;
            }

            em.healths[target].current -= damage;
            attackSucceeded = true;

            if (em.healths[target].current <= 0.0f) {
                GiveLootToInventory(i, target, em);
                em.DestroyEntity(target);
            }
        }

        if (attackSucceeded) {
            behavior.stateTimer = ATTACK_COOLDOWN;
        }
    } else if (behavior.currentTask == "eating") {
        if (em.hasNeeds[i] && em.hasInventory[i] && !behavior.currentItemTarget.empty()) {
            ConsumeFoodFromInventory(em.inventories[i], em.needs[i], behavior.currentItemTarget, resourceReg);
        }
    } else if (behavior.currentTask == "eating_from_storage") {
        EntityID storage = behavior.currentJobTarget;

        if (storage < em.active.size() && em.active[storage] && em.hasInventory[storage] && em.hasNeeds[i] &&
            !behavior.currentItemTarget.empty()) {
            ConsumeFoodFromInventory(em.inventories[storage], em.needs[i], behavior.currentItemTarget, resourceReg);
        }
    }

    ResetBehaviorState(behavior);
}

// ============================================================================
// HELPERS
// ============================================================================

bool AISystem::HasCapability(const BehaviorComponent& behavior, const std::string& capability) const {
    return std::find(behavior.innateCapabilities.begin(), behavior.innateCapabilities.end(), capability) !=
           behavior.innateCapabilities.end();
}

const BehaviorRule* AISystem::FindBehaviorRule(const BehaviorComponent& behavior, const std::string& ruleName) const {
    for (const BehaviorRule& rule : behavior.innateBehaviorRules) {
        if (rule.name == ruleName) {
            return &rule;
        }
    }

    return nullptr;
}

bool AISystem::IsSpeciesTargetedByRule(const BehaviorRule& rule, const std::string& species) const {
    for (const std::string& targetSpecies : rule.arguments) {
        if (targetSpecies == species) {
            return true;
        }
    }

    return false;
}

bool AISystem::AreEntitiesAdjacent(EntityID a, EntityID b, const EntityManager& em) const {
    if (a >= em.active.size() || b >= em.active.size() || !em.active[a] || !em.active[b] || !em.hasTransform[a] || !em.hasTransform[b]) {
        return false;
    }

    const int ax = ToTileCoord(em.transforms[a].position.x);
    const int ay = ToTileCoord(em.transforms[a].position.y);

    const int bx = ToTileCoord(em.transforms[b].position.x);
    const int by = ToTileCoord(em.transforms[b].position.y);

    const int dx = std::abs(ax - bx);
    const int dy = std::abs(ay - by);

    return std::max(dx, dy) <= 1;
}

void AISystem::ResetBehaviorState(BehaviorComponent& behavior) {
    behavior.currentTask = "idle";
    behavior.hasJob = false;
    behavior.currentJobTarget = 0;
    behavior.currentItemTarget.clear();
    behavior.currentPath.clear();
    behavior.currentPathIndex = 0;
    behavior.isMoving = false;
}

// ============================================================================
// JOB SEARCHERS
// ============================================================================

bool AISystem::TryFindHuntJob(EntityID hunter, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                              const EntitySpatialGrid& spatialGrid) {
    // Vérification initiale de l'entité chasseur
    if (hunter >= em.active.size() || !em.active[hunter] || !em.hasBehavior[hunter] || !em.hasTransform[hunter]) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[hunter];

    const BehaviorRule* huntRule = FindBehaviorRule(behavior, "hunt");

    if (huntRule == nullptr || huntRule->arguments.empty()) {
        return false;
    }

    const float searchRadius = GetActionRadiusWorld(hunter, em);
    const Vector2 hunterPosition = em.transforms[hunter].position;

    // Récupération des candidats via la grid
    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(hunterPosition, searchRadius, em);

    EntityID bestTarget = static_cast<EntityID>(-1);
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID target : candidates) {
        if (target == hunter) {
            continue;
        }

        // Vérification de sécurité pour le target (ajoutée par l'autre IA)
        if (target >= em.active.size() || !em.active[target] || !em.hasTag[target] || !em.hasTransform[target] || !em.hasHealth[target]) {
            continue;
        }

        if (em.healths[target].current <= 0.0f) {
            continue;
        }

        const std::string& targetSpecies = em.tags[target].species;

        if (!IsSpeciesTargetedByRule(*huntRule, targetSpecies)) {
            continue;
        }

        const float distanceSq = SquaredDistance(hunterPosition, em.transforms[target].position);

        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestTarget = target;
        }
    }

    // Si aucune cible valide trouvée
    if (bestTarget == static_cast<EntityID>(-1)) {
        return false;
    }

    // Gestion de l'attaque si adjacent
    if (AreEntitiesAdjacent(hunter, bestTarget, em)) {
        behavior.currentTask = "attacking";
        behavior.currentJobTarget = bestTarget;
        behavior.hasJob = true;
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.stateTimer = ATTACK_DURATION;
        return true;
    }

    // Calcul du chemin pour se déplacer vers la cible
    std::vector<Vector2> path =
        Pathfinder::FindPathToAdjacentTile(em.transforms[hunter].position, em.transforms[bestTarget].position, map, tileReg, em, hunter);

    if (path.empty()) {
        return false;
    }

    behavior.currentTask = "moving_to_hunt";
    behavior.currentJobTarget = bestTarget;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;

    return true;
}

bool AISystem::TryFindBuildJob(EntityID i, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                               const EntitySpatialGrid& spatialGrid) {
    if (i >= em.active.size() || !em.active[i] || !em.hasTransform[i]) {
        return false;
    }

    const float searchRadius = GetActionRadiusWorld(i, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[i].position, searchRadius, em);

    for (EntityID j : candidates) {
        if (j >= em.active.size() || !em.active[j] || !em.hasBlueprint[j] || em.blueprints[j].isFinished || !em.hasTransform[j]) {
            continue;
        }

        bool canAfford = true;

        if (em.hasInventory[i]) {
            for (const auto& req : em.blueprints[j].requiredMaterials) {
                if (em.inventories[i].items[req.first] < req.second) {
                    canAfford = false;
                    break;
                }
            }
        }

        if (!canAfford) {
            continue;
        }

        std::vector<Vector2> path =
            Pathfinder::FindPathToAdjacentTile(em.transforms[i].position, em.transforms[j].position, map, tileReg, em, i);

        if (path.empty()) {
            continue;
        }

        auto& behavior = em.behaviors[i];

        behavior.currentTask = "moving_to_build";
        behavior.currentJobTarget = j;
        behavior.hasJob = true;
        behavior.currentPath = std::move(path);
        behavior.currentPathIndex = 0;
        behavior.currentTarget = behavior.currentPath[0];
        behavior.isMoving = true;

        return true;
    }

    return false;
}

bool AISystem::TryFindDismantleJob(EntityID i, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                   const EntitySpatialGrid& spatialGrid) {
    if (i >= em.active.size() || !em.active[i] || !em.hasTransform[i]) {
        return false;
    }

    const float searchRadius = GetActionRadiusWorld(i, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[i].position, searchRadius, em);

    for (EntityID j : candidates) {
        if (j >= em.active.size() || !em.active[j] || !em.hasDeconstruct[j] || !em.hasTransform[j]) {
            continue;
        }

        std::vector<Vector2> path =
            Pathfinder::FindPathToAdjacentTile(em.transforms[i].position, em.transforms[j].position, map, tileReg, em, i);

        if (path.empty()) {
            continue;
        }

        auto& behavior = em.behaviors[i];

        behavior.currentTask = "moving_to_dismantle";
        behavior.currentJobTarget = j;
        behavior.hasJob = true;
        behavior.currentPath = std::move(path);
        behavior.currentPathIndex = 0;
        behavior.currentTarget = behavior.currentPath[0];
        behavior.isMoving = true;

        return true;
    }

    return false;
}

bool AISystem::TryFindWanderJob(EntityID i, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg) {
    auto& behavior = em.behaviors[i];

    constexpr int MaxAttempts = 8;

    for (int attempt = 0; attempt < MaxAttempts; ++attempt) {
        const float angle = GetRandomValue(0, 360) * DEG2RAD;

        const float distance =
            static_cast<float>(GetRandomValue(static_cast<int>(Config::TILE_SIZE * 2), static_cast<int>(Config::TILE_SIZE * 8)));

        Vector2 proposedTarget = {em.transforms[i].position.x + std::cos(angle) * distance,
                                  em.transforms[i].position.y + std::sin(angle) * distance};

        std::vector<Vector2> path = Pathfinder::FindPath(em.transforms[i].position, proposedTarget, map, tileReg, em, i);

        if (path.empty()) {
            continue;
        }

        behavior.currentTask = "wandering";
        behavior.currentPath = std::move(path);
        behavior.currentPathIndex = 0;
        behavior.currentTarget = behavior.currentPath[0];
        behavior.isMoving = true;

        return true;
    }

    behavior.stateTimer = 1.0f;
    return false;
}

bool AISystem::TryFindHarvestJob(EntityID worker, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                 const EntitySpatialGrid& spatialGrid) {
    if (worker >= em.active.size() || !em.active[worker] || !em.hasBehavior[worker] || !em.hasTransform[worker]) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[worker];
    const BehaviorRule* harvestRule = FindBehaviorRule(behavior, "harvest");

    if (harvestRule == nullptr || harvestRule->arguments.empty()) {
        return false;
    }

    const float searchRadius = GetActionRadiusWorld(worker, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[worker].position, searchRadius, em);

    EntityID bestTarget = static_cast<EntityID>(-1);
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    const Vector2 workerPosition = em.transforms[worker].position;

    for (EntityID target : candidates) {
        if (target == worker || target >= em.active.size() || !em.active[target] || !em.hasHarvestable[target] ||
            !em.hasTransform[target] || !em.hasTag[target]) {
            continue;
        }

        const std::string& targetPrefab = em.tags[target].prefabId;

        bool isTargeted = false;

        for (const std::string& arg : harvestRule->arguments) {
            if (arg == targetPrefab) {
                isTargeted = true;
                break;
            }
        }

        if (!isTargeted) {
            continue;
        }

        const float distanceSq = SquaredDistance(workerPosition, em.transforms[target].position);

        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestTarget = target;
        }
    }

    if (bestTarget == static_cast<EntityID>(-1)) {
        return false;
    }

    if (AreEntitiesAdjacent(worker, bestTarget, em)) {
        behavior.currentTask = "harvesting";
        behavior.currentJobTarget = bestTarget;
        behavior.hasJob = true;
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.stateTimer = 3.0f;
        return true;
    }

    std::vector<Vector2> path =
        Pathfinder::FindPathToAdjacentTile(workerPosition, em.transforms[bestTarget].position, map, tileReg, em, worker);

    if (path.empty()) {
        return false;
    }

    behavior.currentTask = "moving_to_harvest";
    behavior.currentJobTarget = bestTarget;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;

    return true;
}

bool AISystem::TryFindSeekFoodJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                  const ResourceRegistry& resourceReg, const EntitySpatialGrid& spatialGrid) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasNeeds[entity] || !em.hasInventory[entity] || !em.hasBehavior[entity] ||
        !em.hasTransform[entity]) {
        return false;
    }

    auto& needs = em.needs[entity];

    const float hungerThreshold = needs.maxHunger * SEEK_FOOD_THRESHOLD_RATIO;

    if (needs.hunger >= hungerThreshold) {
        return false;
    }

    auto& inventory = em.inventories[entity];
    auto& behavior = em.behaviors[entity];

    // =========================================================
    // 1. Eat from own inventory.
    // =========================================================
    const std::string ownFood = FindFirstFoodItemInInventory(inventory, resourceReg);

    if (!ownFood.empty()) {
        behavior.currentTask = "eating";
        behavior.currentItemTarget = ownFood;
        behavior.hasJob = true;
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.stateTimer = EAT_DURATION;

        return true;
    }

    const float searchRadius = GetActionRadiusWorld(entity, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, searchRadius, em);

    // =========================================================
    // 2. Search nearby completed storage with food.
    // =========================================================
    EntityID bestStorage = static_cast<EntityID>(-1);
    std::string bestStorageFood;
    float bestStorageDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (candidate == entity || candidate >= em.active.size() || !em.active[candidate] || !em.hasTransform[candidate] ||
            !em.hasInventory[candidate]) {
            continue;
        }

        // Do not eat from unfinished blueprints.
        if (em.hasBlueprint[candidate] && !em.blueprints[candidate].isFinished) {
            continue;
        }

        // Avoid stealing from other creatures for now.
        // Storage furniture has inventory but no behavior.
        if (em.hasBehavior[candidate]) {
            continue;
        }

        const std::string foodItem = FindFirstFoodItemInInventory(em.inventories[candidate], resourceReg);

        if (foodItem.empty()) {
            continue;
        }

        const float distanceSq = SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        if (distanceSq < bestStorageDistanceSq) {
            bestStorageDistanceSq = distanceSq;
            bestStorage = candidate;
            bestStorageFood = foodItem;
        }
    }

    if (bestStorage != static_cast<EntityID>(-1)) {
        if (AreEntitiesAdjacent(entity, bestStorage, em)) {
            behavior.currentTask = "eating_from_storage";
            behavior.currentJobTarget = bestStorage;
            behavior.currentItemTarget = bestStorageFood;
            behavior.hasJob = true;
            behavior.isMoving = false;
            behavior.currentPath.clear();
            behavior.currentPathIndex = 0;
            behavior.stateTimer = EAT_DURATION;

            return true;
        }

        std::vector<Vector2> path = Pathfinder::FindPathToAdjacentTile(em.transforms[entity].position, em.transforms[bestStorage].position,
                                                                       map, tileReg, em, entity);

        if (!path.empty()) {
            behavior.currentTask = "moving_to_food_storage";
            behavior.currentJobTarget = bestStorage;
            behavior.currentItemTarget = bestStorageFood;
            behavior.hasJob = true;
            behavior.currentPath = std::move(path);
            behavior.currentPathIndex = 0;
            behavior.currentTarget = behavior.currentPath[0];
            behavior.isMoving = true;

            return true;
        }
    }

    // =========================================================
    // 3. Search nearby harvestable food source.
    // Example: BUSH_BERRY with consumable BUSH_BERRY drop.
    // =========================================================
    EntityID bestFoodSource = static_cast<EntityID>(-1);
    float bestFoodSourceDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (candidate == entity || candidate >= em.active.size() || !em.active[candidate] || !em.hasTransform[candidate] ||
            !em.hasHarvestable[candidate] || !em.hasHealth[candidate]) {
            continue;
        }

        const HarvestableComponent& harvestable = em.harvestables[candidate];

        if (!HarvestableHasFoodDrop(harvestable, resourceReg)) {
            continue;
        }

        if (!HasRequiredHarvestTool(entity, harvestable, em)) {
            continue;
        }

        const float distanceSq = SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        if (distanceSq < bestFoodSourceDistanceSq) {
            bestFoodSourceDistanceSq = distanceSq;
            bestFoodSource = candidate;
        }
    }

    if (bestFoodSource == static_cast<EntityID>(-1)) {
        return false;
    }

    if (AreEntitiesAdjacent(entity, bestFoodSource, em)) {
        behavior.currentTask = "harvesting";
        behavior.currentJobTarget = bestFoodSource;
        behavior.hasJob = true;
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.stateTimer = 1.0f;
        behavior.actionAccumulator = 0.0f;

        return true;
    }

    std::vector<Vector2> path = Pathfinder::FindPathToAdjacentTile(em.transforms[entity].position, em.transforms[bestFoodSource].position,
                                                                   map, tileReg, em, entity);

    if (path.empty()) {
        return false;
    }

    behavior.currentTask = "moving_to_harvest";
    behavior.currentJobTarget = bestFoodSource;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;

    return true;
}

bool AISystem::TryFindStoreJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                               const EntitySpatialGrid& spatialGrid) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasInventory[entity] || !em.hasBehavior[entity] ||
        !em.hasTransform[entity]) {
        return false;
    }

    InventoryComponent& inventory = em.inventories[entity];

    if (!HasAnyInventoryItem(inventory)) {
        return false;
    }

    const float searchRadius = GetActionRadiusWorld(entity, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, searchRadius, em);

    EntityID bestStorage = static_cast<EntityID>(-1);
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (candidate == entity || candidate >= em.active.size() || !em.active[candidate] || !em.hasTransform[candidate] ||
            !em.hasInventory[candidate] || !em.hasStorage[candidate]) {
            continue;
        }

        // Do not store into unfinished blueprints.
        if (em.hasBlueprint[candidate] && !em.blueprints[candidate].isFinished) {
            continue;
        }

        // Avoid depositing into another creature inventory.
        if (em.hasBehavior[candidate]) {
            continue;
        }

        if (!StorageCanAcceptFromInventory(candidate, inventory, em)) {
            continue;
        }
        const float distanceSq = SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestStorage = candidate;
        }
    }

    if (bestStorage == static_cast<EntityID>(-1)) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[entity];

    if (AreEntitiesAdjacent(entity, bestStorage, em)) {
        behavior.currentTask = "depositing";
        behavior.currentJobTarget = bestStorage;
        behavior.hasJob = true;
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.stateTimer = 0.8f;

        return true;
    }

    std::vector<Vector2> path =
        Pathfinder::FindPathToAdjacentTile(em.transforms[entity].position, em.transforms[bestStorage].position, map, tileReg, em, entity);

    if (path.empty()) {
        return false;
    }

    behavior.currentTask = "moving_to_storage";
    behavior.currentJobTarget = bestStorage;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;

    return true;
}
