/**
 * @file AISystemTasks.cpp
 * @brief Processes the completion and mechanical execution of active AI tasks.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/AISystem.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/Pathfinder.hpp"
#include "systems/VillageRequestSystem.hpp"
#include "systems/ai/AICompletion.hpp"
#include "systems/ai/AICompletionContext.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

std::unordered_map<std::string, int> GetCraftRequirementsForTask(const std::string& itemId) {
    if (itemId == "SPEAR") {
        return {{"WOOD", 4}, {"ROPE", 1}};
    }

    if (itemId == "IRON_AXE") {
        return {{"WOOD", 2}, {"IRON_INGOT", 1}};
    }

    if (itemId == "WOOD_BOW") {
        return {{"WOOD", 6}, {"ROPE", 2}};
    }

    return {};
}

EntityID FindNearestCompatibleStorage(EntityID worker, const std::string& itemId, const EntityManager& em,
                                      const EntitySpatialGrid& spatialGrid) {
    if (worker >= em.active.size() || !em.active[worker] || !em.hasTransform[worker]) {
        return static_cast<EntityID>(-1);
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(worker, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[worker].position, searchRadius, em);

    EntityID bestStorage = static_cast<EntityID>(-1);
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (candidate >= em.active.size() || !em.active[candidate] || !em.hasTransform[candidate] || !em.hasInventory[candidate] ||
            !em.hasStorage[candidate]) {
            continue;
        }

        if (em.hasBlueprint[candidate] && !em.blueprints[candidate].isFinished) {
            continue;
        }

        if (em.hasBehavior[candidate]) {
            continue;
        }

        if (!AISystemUtils::StorageAcceptsItem(em.storages[candidate], itemId)) {
            continue;
        }

        if (!AISystemUtils::HasAvailableStorageCapacity(candidate, em)) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[worker].position, em.transforms[candidate].position);

        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestStorage = candidate;
        }
    }

    return bestStorage;
}

} // namespace

void AISystem::HandleTaskCompletion(EntityID i, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                    const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                    const EntitySpatialGrid& spatialGrid, RoomSystem& roomSys) {
    auto& behavior = em.behaviors[i];

    const AICompletionContext completionContext{i, em, map, tileReg, resourceReg, weaponReg, spatialGrid, roomSys};

    if (ai::completion::TryCompleteNeedsTask(completionContext) || ai::completion::TryCompleteStorageTask(completionContext) ||
        ai::completion::TryCompleteCraftingTask(completionContext) || ai::completion::TryCompleteReligionTask(completionContext) ||
        ai::completion::TryCompleteSocialTask(completionContext) || ai::completion::TryCompleteHostilityTask(completionContext)) {
        ResetBehaviorState(behavior);
        return;
    }

    if (behavior.currentTask == "building") {
        EntityID target = behavior.currentJobTarget;

        if (target < em.active.size() && em.active[target] && em.hasBlueprint[target]) {
            if (AISystemUtils::ConsumeAccessibleMaterials(i, em, spatialGrid, em.blueprints[target].requiredMaterials)) {
                em.blueprints[target].isFinished = true;
                em.hasBlueprint[target] = false;

                if (em.hasConstruction[target] || (em.hasTag[target] && !em.hasBehavior[target])) {
                    roomSys.MarkDirty();
                }
            }

            if (em.hasConstruction[target] || (em.hasTag[target] && !em.hasBehavior[target])) {
                roomSys.MarkDirty();
            }
        }
    } else if (behavior.currentTask == "requesting_weapon") {
        const EntityID blacksmith = behavior.currentJobTarget;
        const std::string requestedWeapon = behavior.currentItemTarget.empty() ? "SPEAR" : behavior.currentItemTarget;

        if (blacksmith < em.active.size() && em.active[blacksmith] && em.hasVillageMember[i]) {
            const EntityID villageId = em.villageMembers[i].villageId;

            EntityID requestId =
                VillageRequestSystem::CreateRequest(em, VillageRequestType::WeaponNeeded, i, villageId, requestedWeapon, 1, 750.0f);

            if (requestId != static_cast<EntityID>(-1)) {
                VillageRequestSystem::AssignRequest(em, requestId, blacksmith);
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
            }
            // else {
            //     // 5. L'arbre est encore en vie ! On boucle.
            //     behavior.stateTimer = 1.0f; // Prochain coup de hache dans 1 seconde
            //     return;                     // TRÈS IMPORTANT : On sort pour NE PAS appeler le ResetBehaviorState() global !
            // }
        }
    } else if (behavior.currentTask == "equipping_weapon") {
        EntityID storage = behavior.currentJobTarget;
        const std::string weaponId = behavior.currentItemTarget;

        const WeaponDef* weaponDef = weaponReg.GetWeaponDef(weaponId);

        if (weaponDef != nullptr && storage < em.active.size() && em.active[storage] && em.hasInventory[storage] && !weaponId.empty()) {
            const int removed = AISystemUtils::RemoveItemFromInventory(em.inventories[storage], weaponId, 1);

            if (removed > 0) {
                if (!em.hasEquipment[i]) {
                    em.hasEquipment[i] = true;
                    em.equipments[i] = {};
                }

                EquipmentComponent& equipment = em.equipments[i];

                equipment.rightHandItemId = weaponDef->id;
                equipment.rightHandToolType = weaponDef->toolType;
                equipment.rightHandDamage = weaponDef->damage;
                equipment.equipmentSlot = weaponDef->equipmentSlot;
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

            if (em.hasAIContext[target]) {
                em.aiContexts[target].lastThreatId = i;
                em.aiContexts[target].threatMemoryTimer = 8.0f;
            }

            if (em.healths[target].current <= 0.0f) {
                AISystemUtils::GiveLootToInventory(i, target, em);
                em.DestroyEntity(target);
            }
        }

        if (attackSucceeded) {
            behavior.stateTimer = AISystemUtils::ATTACK_COOLDOWN;
        }
    }

    ResetBehaviorState(behavior);
}
