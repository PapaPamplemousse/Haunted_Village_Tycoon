#include "systems/AISystem.hpp"
#include "systems/AISystemUtils.hpp"

#include <algorithm>

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
                AISystemUtils::DepositInventoryIntoStorage(em.inventories[i], em.inventories[storage], em.storages[storage]);
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
    }

    ResetBehaviorState(behavior);
}
