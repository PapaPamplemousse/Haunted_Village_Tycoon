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

RelationshipEntry& GetOrCreateTaskRelationship(EntityManager& em, EntityID owner, EntityID other) {
    SocialComponent& social = em.socials[owner];

    for (RelationshipEntry& relationship : social.relationships) {
        if (relationship.otherId == other) {
            return relationship;
        }
    }

    social.relationships.push_back({other});
    return social.relationships.back();
}

float ClampSocialTaskValue(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }

    if (value > 100.0f) {
        return 100.0f;
    }

    return value;
}

float GetTaskKindness(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasPersonality[entity]) {
        return 0.5f;
    }

    return em.personalities[entity].kindness;
}

float GetTaskAggression(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasPersonality[entity]) {
        return 0.0f;
    }

    return em.personalities[entity].aggression;
}

float GetTaskPatience(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasPersonality[entity]) {
        return 0.5f;
    }

    return em.personalities[entity].patience;
}

RelationshipEntry& GetOrCreateHostilityRelationship(EntityManager& em, EntityID owner, EntityID other) {
    SocialComponent& social = em.socials[owner];

    for (RelationshipEntry& relationship : social.relationships) {
        if (relationship.otherId == other) {
            return relationship;
        }
    }

    social.relationships.push_back({other});
    return social.relationships.back();
}

float ClampSocialHostility(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }

    if (value > 100.0f) {
        return 100.0f;
    }

    return value;
}

float GetHostilityAggression(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasPersonality[entity]) {
        return 0.0f;
    }

    return em.personalities[entity].aggression;
}

float GetHostilityBravery(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasPersonality[entity]) {
        return 0.5f;
    }

    return em.personalities[entity].bravery;
}

bool HasHostilityTrait(const EntityManager& em, EntityID entity, const std::string& traitId) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasPersonality[entity]) {
        return false;
    }

    const std::vector<std::string>& traits = em.personalities[entity].traits;

    return std::find(traits.begin(), traits.end(), traitId) != traits.end();
}

RelationshipEntry& GetOrCreateReligionRelationship(EntityManager& em, EntityID owner, EntityID other) {
    SocialComponent& social = em.socials[owner];

    for (RelationshipEntry& relationship : social.relationships) {
        if (relationship.otherId == other) {
            return relationship;
        }
    }

    social.relationships.push_back({other});
    return social.relationships.back();
}

float ClampReligionValue(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }

    if (value > 100.0f) {
        return 100.0f;
    }

    return value;
}

bool IsHumanReligionTarget(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasTag[entity] && em.tags[entity].species == "human";
}

} // namespace

void AISystem::HandleTaskCompletion(EntityID i, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                    const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                    const EntitySpatialGrid& spatialGrid, RoomSystem& roomSys) {
    auto& behavior = em.behaviors[i];

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
    } else if (behavior.currentTask == "resting") {
        if (em.hasNeeds[i]) {
            auto& needs = em.needs[i];

            needs.fatigue -= Config::FATIGUE_REST_RECOVERY_PER_SECOND;

            if (needs.fatigue < 0.0f) {
                needs.fatigue = 0.0f;
            }

            if (!AISystemUtils::IsFullyRested(i, em)) {
                behavior.stateTimer = 1.0f;
                return;
            }
        }

        AISystemUtils::ReleaseRestSpotReservation(i, em);
    } else if (behavior.currentTask == "repairing") {
        EntityID target = behavior.currentJobTarget;

        if (target < em.active.size() && em.active[target] && em.hasHealth[target]) {
            em.healths[target].current = std::min(em.healths[target].max, em.healths[target].current + 25.0f);

            if (em.healths[target].current < em.healths[target].max) {
                behavior.stateTimer = 1.0f;
                return;
            }
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
    } else if (behavior.currentTask == "hauling_deposit") {
        if (em.hasAIContext[i] && em.hasInventory[i]) {
            AIContextComponent& context = em.aiContexts[i];

            const EntityID destination = context.haulDestinationId;
            const std::string itemId = context.haulItemId;

            if (destination < em.active.size() && em.active[destination] && em.hasInventory[destination] && em.hasStorage[destination] &&
                !itemId.empty()) {
                const int remainingCapacity =
                    em.storages[destination].capacity - AISystemUtils::GetInventoryItemCount(em.inventories[destination]);

                if (remainingCapacity > 0) {
                    const int amount =
                        AISystemUtils::RemoveItemFromInventory(em.inventories[i], itemId, std::min(context.haulAmount, remainingCapacity));

                    if (amount > 0) {
                        em.inventories[destination].items[itemId] += amount;
                    }
                }
            }

            context.haulSourceId = static_cast<EntityID>(-1);
            context.haulDestinationId = static_cast<EntityID>(-1);
            context.haulItemId.clear();
            context.haulAmount = 0;
        }
    } else if (behavior.currentTask == "depositing") {
        EntityID storage = behavior.currentJobTarget;

        if (storage < em.active.size() && em.active[storage] && em.hasInventory[i] && em.hasInventory[storage] && em.hasStorage[storage]) {
            // Do not deposit into unfinished blueprints.
            if (!(em.hasBlueprint[storage] && !em.blueprints[storage].isFinished)) {
                AISystemUtils::DepositInventoryIntoStorage(em.inventories[i], em.inventories[storage], em.storages[storage]);
            }
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
    } else if (behavior.currentTask == "crafting_weapon") {
        if (em.hasAIContext[i] && em.hasInventory[i]) {
            AIContextComponent& context = em.aiContexts[i];

            const EntityID requestId = context.activeRequestId;
            const std::string itemId = context.requestedCraftItemId.empty() ? behavior.currentItemTarget : context.requestedCraftItemId;

            const std::unordered_map<std::string, int> requirements = GetCraftRequirementsForTask(itemId);

            if (itemId.empty() || requirements.empty() || !AISystemUtils::ConsumeAccessibleMaterials(i, em, spatialGrid, requirements)) {
                if (requestId < em.active.size() && em.active[requestId] && em.hasVillageRequest[requestId]) {
                    VillageRequestSystem::CancelRequest(em, requestId);
                }

                context.activeRequestId = static_cast<EntityID>(-1);
                context.requestedCraftItemId.clear();
            } else {
                em.inventories[i].items[itemId] += 1;

                EntityID storage = FindNearestCompatibleStorage(i, itemId, em, spatialGrid);

                if (storage == static_cast<EntityID>(-1)) {
                    if (requestId < em.active.size() && em.active[requestId] && em.hasVillageRequest[requestId]) {
                        VillageRequestSystem::CancelRequest(em, requestId);
                    }

                    context.activeRequestId = static_cast<EntityID>(-1);
                    context.requestedCraftItemId.clear();
                } else if (AreEntitiesAdjacent(i, storage, em)) {
                    behavior.currentTask = "depositing_crafted_weapon";
                    behavior.currentJobTarget = storage;
                    behavior.currentItemTarget = itemId;
                    behavior.stateTimer = AISystemUtils::DEPOSIT_DURATION;
                    return;
                } else {
                    std::vector<Vector2> path =
                        Pathfinder::FindPathToAdjacentTile(em.transforms[i].position, em.transforms[storage].position, map, tileReg, em, i);

                    if (path.empty()) {
                        if (requestId < em.active.size() && em.active[requestId] && em.hasVillageRequest[requestId]) {
                            VillageRequestSystem::CancelRequest(em, requestId);
                        }

                        context.activeRequestId = static_cast<EntityID>(-1);
                        context.requestedCraftItemId.clear();
                    } else {
                        behavior.currentTask = "moving_to_crafted_weapon_storage";
                        behavior.currentJobTarget = storage;
                        behavior.currentItemTarget = itemId;
                        behavior.hasJob = true;
                        behavior.currentPath = std::move(path);
                        behavior.currentPathIndex = 0;
                        behavior.currentTarget = behavior.currentPath[0];
                        behavior.isMoving = true;
                        behavior.stateTimer = 0.0f;
                        return;
                    }
                }
            }
        }
    } else if (behavior.currentTask == "depositing_crafted_weapon") {
        if (em.hasAIContext[i] && em.hasInventory[i]) {
            AIContextComponent& context = em.aiContexts[i];

            const EntityID requestId = context.activeRequestId;
            const EntityID destination = behavior.currentJobTarget;
            const std::string itemId = behavior.currentItemTarget.empty() ? context.requestedCraftItemId : behavior.currentItemTarget;

            bool deposited = false;

            if (destination < em.active.size() && em.active[destination] && em.hasInventory[destination] && em.hasStorage[destination] &&
                !itemId.empty() && AISystemUtils::StorageAcceptsItem(em.storages[destination], itemId) &&
                AISystemUtils::HasAvailableStorageCapacity(destination, em)) {
                const int removed = AISystemUtils::RemoveItemFromInventory(em.inventories[i], itemId, 1);

                if (removed > 0) {
                    em.inventories[destination].items[itemId] += removed;
                    deposited = true;
                }
            }

            if (requestId < em.active.size() && em.active[requestId] && em.hasVillageRequest[requestId]) {
                if (deposited) {
                    VillageRequestSystem::CompleteRequest(em, requestId);
                } else {
                    VillageRequestSystem::CancelRequest(em, requestId);
                }
            }

            context.activeRequestId = static_cast<EntityID>(-1);
            context.requestedCraftItemId.clear();
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
    } else if (behavior.currentTask == "eating") {
        if (em.hasNeeds[i] && em.hasInventory[i] && !behavior.currentItemTarget.empty()) {
            AISystemUtils::ConsumeFoodFromInventory(em.inventories[i], em.needs[i], behavior.currentItemTarget, resourceReg);
        }
    } else if (behavior.currentTask == "eating_from_storage") {
        EntityID storage = behavior.currentJobTarget;

        if (storage < em.active.size() && em.active[storage] && em.hasInventory[storage] && em.hasNeeds[i] &&
            !behavior.currentItemTarget.empty()) {
            AISystemUtils::ConsumeFoodFromInventory(em.inventories[storage], em.needs[i], behavior.currentItemTarget, resourceReg);
        }
    } else if (behavior.currentTask == "feeding_child") {
        EntityID child = behavior.currentJobTarget;

        if (child < em.active.size() && em.active[child] && em.hasNeeds[child] && em.hasInventory[i] &&
            !behavior.currentItemTarget.empty()) {
            AISystemUtils::ConsumeFoodFromInventory(em.inventories[i], em.needs[child], behavior.currentItemTarget, resourceReg);
        }

        if (em.hasAIContext[i]) {
            em.aiContexts[i].careTargetId = static_cast<EntityID>(-1);
        }
    } else if (behavior.currentTask == "socializing") {
        EntityID target = behavior.currentJobTarget;

        if (target < em.active.size() && em.active[target] && em.hasSocial[i] && em.hasSocial[target]) {
            RelationshipEntry& relToTarget = GetOrCreateTaskRelationship(em, i, target);
            RelationshipEntry& relFromTarget = GetOrCreateTaskRelationship(em, target, i);

            const float kindnessA = GetTaskKindness(em, i);
            const float kindnessB = GetTaskKindness(em, target);

            relToTarget.friendship = ClampSocialTaskValue(relToTarget.friendship + 4.0f + kindnessA * 3.0f);
            relFromTarget.friendship = ClampSocialTaskValue(relFromTarget.friendship + 3.0f + kindnessB * 2.0f);

            relToTarget.trust = ClampSocialTaskValue(relToTarget.trust + 1.0f);
            relFromTarget.trust = ClampSocialTaskValue(relFromTarget.trust + 1.0f);

            relToTarget.resentment = ClampSocialTaskValue(relToTarget.resentment - 2.0f);
            relFromTarget.resentment = ClampSocialTaskValue(relFromTarget.resentment - 1.0f);
        }
    } else if (behavior.currentTask == "confronting_person") {
        EntityID target = behavior.currentJobTarget;

        if (target < em.active.size() && em.active[target] && em.hasSocial[i] && em.hasSocial[target]) {
            RelationshipEntry& relToTarget = GetOrCreateTaskRelationship(em, i, target);
            RelationshipEntry& relFromTarget = GetOrCreateTaskRelationship(em, target, i);

            const float aggression = GetTaskAggression(em, i);
            const float patience = GetTaskPatience(em, i);

            // V1 non-lethal confrontation:
            // - aggressive/impatient villagers escalate resentment and fear;
            // - patient villagers slightly reduce their own resentment.
            const bool escalates = aggression > 0.45f || patience < 0.35f;

            if (escalates) {
                relToTarget.resentment = ClampSocialTaskValue(relToTarget.resentment + 4.0f + aggression * 6.0f);
                relFromTarget.resentment = ClampSocialTaskValue(relFromTarget.resentment + 6.0f + aggression * 4.0f);
                relFromTarget.fear = ClampSocialTaskValue(relFromTarget.fear + aggression * 6.0f);
                relToTarget.friendship = ClampSocialTaskValue(relToTarget.friendship - 2.0f);
                relFromTarget.friendship = ClampSocialTaskValue(relFromTarget.friendship - 3.0f);
            } else {
                relToTarget.resentment = ClampSocialTaskValue(relToTarget.resentment - 5.0f);
                relFromTarget.resentment = ClampSocialTaskValue(relFromTarget.resentment + 1.0f);
                relToTarget.trust = ClampSocialTaskValue(relToTarget.trust + 1.0f);
            }
        }

    } else if (behavior.currentTask == "intimidating_person") {
        EntityID target = behavior.currentJobTarget;

        if (target < em.active.size() && em.active[target] && em.hasSocial[i] && em.hasSocial[target]) {
            RelationshipEntry& actorToTarget = GetOrCreateHostilityRelationship(em, i, target);
            RelationshipEntry& targetToActor = GetOrCreateHostilityRelationship(em, target, i);

            const float aggression = GetHostilityAggression(em, i);
            const float bravery = GetHostilityBravery(em, i);

            actorToTarget.resentment = ClampSocialHostility(actorToTarget.resentment - 2.0f);
            actorToTarget.respect = ClampSocialHostility(actorToTarget.respect + 1.0f);

            targetToActor.fear = ClampSocialHostility(targetToActor.fear + 8.0f + aggression * 8.0f + bravery * 3.0f);
            targetToActor.resentment = ClampSocialHostility(targetToActor.resentment + 3.0f);
            targetToActor.friendship = ClampSocialHostility(targetToActor.friendship - 2.0f);

            std::cout << "[HOSTILITY] Entity #" << i << " intimidated entity #" << target << "." << std::endl;

            if (em.hasAIContext[i]) {
                em.aiContexts[i].socialActionCooldownTimer = 20.0f;
            }
        }
    } else if (behavior.currentTask == "fighting_non_lethal") {
        EntityID target = behavior.currentJobTarget;

        if (target < em.active.size() && em.active[target] && em.hasHealth[target] && em.hasSocial[i] && em.hasSocial[target]) {
            RelationshipEntry& actorToTarget = GetOrCreateHostilityRelationship(em, i, target);
            RelationshipEntry& targetToActor = GetOrCreateHostilityRelationship(em, target, i);

            float damage = 3.0f + GetHostilityAggression(em, i) * 7.0f;

            if (em.hasStats[i]) {
                damage += em.stats[i].baseAttack * 0.5f;
            }

            // Non-lethal cap: never reduce below 20% max HP.
            const float minHp = std::max(1.0f, em.healths[target].max * 0.20f);

            em.healths[target].current = std::max(minHp, em.healths[target].current - damage);

            actorToTarget.resentment = ClampSocialHostility(actorToTarget.resentment - 4.0f);
            actorToTarget.fear = ClampSocialHostility(actorToTarget.fear - 2.0f);

            targetToActor.resentment = ClampSocialHostility(targetToActor.resentment + 12.0f);
            targetToActor.fear = ClampSocialHostility(targetToActor.fear + 10.0f);
            targetToActor.friendship = ClampSocialHostility(targetToActor.friendship - 8.0f);
            targetToActor.trust = ClampSocialHostility(targetToActor.trust - 10.0f);

            std::cout << "[HOSTILITY] Entity #" << i << " fought entity #" << target << " non-lethally." << std::endl;

            if (em.hasAIContext[i]) {
                em.aiContexts[i].socialActionCooldownTimer = 120.0f;
            }
        }
    } else if (behavior.currentTask == "murdering_person") {
        EntityID target = behavior.currentJobTarget;

        if (target < em.active.size() && em.active[target] && em.hasHealth[target] && em.hasSocial[i]) {
            const bool darkTrait = HasHostilityTrait(em, i, "VIOLENT") || HasHostilityTrait(em, i, "VENGEFUL");

            if (darkTrait && GetHostilityAggression(em, i) >= 0.85f) {
                em.healths[target].current = 0.0f;
                em.DestroyEntity(target);

                RelationshipEntry& actorToTarget = GetOrCreateHostilityRelationship(em, i, target);
                actorToTarget.resentment = ClampSocialHostility(actorToTarget.resentment - 20.0f);
                actorToTarget.fear = ClampSocialHostility(actorToTarget.fear + 10.0f);

                std::cout << "[HOSTILITY] Entity #" << i << " murdered entity #" << target << "." << std::endl;
            }
        }
    } else if (behavior.currentTask == "praying") {
        if (em.hasFaction[i]) {
            FactionComponent& faction = em.factions[i];

            if (faction.factionId == "COMMON_FOLK") {
                faction.factionId = "OLD_FAITH";
                faction.conviction = std::max(faction.conviction, 15.0f);
            } else if (faction.factionId == "OLD_FAITH") {
                faction.conviction = ClampReligionValue(faction.conviction + 6.0f);
            }
        }
    } else if (behavior.currentTask == "preaching") {
        const EntityID target = behavior.currentJobTarget;

        if (IsHumanReligionTarget(em, target) && em.hasFaction[target] && em.hasSocial[i] && em.hasSocial[target]) {
            FactionComponent& targetFaction = em.factions[target];

            RelationshipEntry& priestToTarget = GetOrCreateReligionRelationship(em, i, target);
            RelationshipEntry& targetToPriest = GetOrCreateReligionRelationship(em, target, i);

            if (targetFaction.factionId == "COMMON_FOLK") {
                targetFaction.factionId = "OLD_FAITH";
                targetFaction.conviction = std::max(targetFaction.conviction, 12.0f);

                targetToPriest.trust = ClampReligionValue(targetToPriest.trust + 6.0f);
                priestToTarget.respect = ClampReligionValue(priestToTarget.respect + 2.0f);
            } else if (targetFaction.factionId == "OLD_FAITH") {
                targetFaction.conviction = ClampReligionValue(targetFaction.conviction + 5.0f);
                targetToPriest.trust = ClampReligionValue(targetToPriest.trust + 3.0f);
            } else if (targetFaction.factionId == "CULT_OF_THE_HOLLOW") {
                targetToPriest.resentment = ClampReligionValue(targetToPriest.resentment + 5.0f);
                targetToPriest.trust = ClampReligionValue(targetToPriest.trust - 3.0f);
                priestToTarget.resentment = ClampReligionValue(priestToTarget.resentment + 2.0f);
            }
        }
    } else if (behavior.currentTask == "holding_ritual") {
        if (em.hasVillageMember[i]) {
            const EntityID villageId = em.villageMembers[i].villageId;

            for (EntityID target = 0; target < em.active.size(); ++target) {
                if (!IsHumanReligionTarget(em, target) || !em.hasVillageMember[target] || !em.hasFaction[target] ||
                    em.villageMembers[target].villageId != villageId) {
                    continue;
                }

                FactionComponent& targetFaction = em.factions[target];

                if (targetFaction.factionId == "OLD_FAITH") {
                    targetFaction.conviction = ClampReligionValue(targetFaction.conviction + 4.0f);
                } else if (targetFaction.factionId == "COMMON_FOLK") {
                    targetFaction.conviction = ClampReligionValue(targetFaction.conviction + 1.0f);
                }

                if (em.hasSocial[target]) {
                    for (RelationshipEntry& relationship : em.socials[target].relationships) {
                        relationship.fear = ClampReligionValue(relationship.fear - 3.0f);
                    }
                }
            }
        }
    } else if (behavior.currentTask == "comforting_frightened") {
        const EntityID target = behavior.currentJobTarget;

        if (IsHumanReligionTarget(em, target) && em.hasSocial[i] && em.hasSocial[target]) {
            RelationshipEntry& priestToTarget = GetOrCreateReligionRelationship(em, i, target);
            RelationshipEntry& targetToPriest = GetOrCreateReligionRelationship(em, target, i);

            targetToPriest.trust = ClampReligionValue(targetToPriest.trust + 6.0f);
            targetToPriest.friendship = ClampReligionValue(targetToPriest.friendship + 3.0f);
            priestToTarget.respect = ClampReligionValue(priestToTarget.respect + 2.0f);

            for (RelationshipEntry& relationship : em.socials[target].relationships) {
                relationship.fear = ClampReligionValue(relationship.fear - 8.0f);
            }

            if (em.hasFaction[target] && em.factions[target].factionId == "OLD_FAITH") {
                em.factions[target].conviction = ClampReligionValue(em.factions[target].conviction + 3.0f);
            }
        }
    }

    ResetBehaviorState(behavior);
}
