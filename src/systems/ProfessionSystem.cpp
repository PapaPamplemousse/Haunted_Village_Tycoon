/**
 * @file ProfessionSystem.cpp
 * @brief Implementation of the profession validation and assignment logic.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/ProfessionSystem.hpp"

#include "core/Config.hpp"

#include <unordered_set>

namespace {

void ApplyBehaviorRules(EntityID entity, EntityManager& em, const BehaviorRegistry& behReg) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasTag[entity] || !em.hasProfession[entity] || !em.hasBehavior[entity]) {
        return;
    }

    const auto& tag = em.tags[entity];
    const auto& prof = em.professions[entity];

    auto newRules = behReg.GetBehaviorsFor(tag.category, tag.species, prof.currentProfession);

    em.behaviors[entity].innateBehaviorRules = newRules;
    em.behaviors[entity].innateCapabilities.clear();

    for (const auto& rule : newRules) {
        em.behaviors[entity].innateCapabilities.push_back(rule.name);
    }
}

void ResetProfessionKeepMode(EntityID entity, EntityManager& em, const BehaviorRegistry& behReg) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasProfession[entity]) {
        return;
    }

    em.professions[entity].currentProfession = "none";
    ApplyBehaviorRules(entity, em, behReg);
}

} // namespace

void ProfessionSystem::Update(float deltaTime, EntityManager& em, const ProfessionRegistry& profReg, const BehaviorRegistry& behReg) {
    m_updateAccumulator += deltaTime;

    if (m_updateAccumulator < Config::PROFESSION_UPDATE_INTERVAL) {
        return;
    }

    m_updateAccumulator = 0.0f;

    std::unordered_set<EntityID> assignedWorkers;

    // =========================================================
    // PASS 1: Validate existing workplace slots.
    // =========================================================
    for (size_t workplaceEntity = 0; workplaceEntity < em.active.size(); ++workplaceEntity) {
        if (!em.active[workplaceEntity] || !em.hasWorkplace[workplaceEntity]) {
            continue;
        }

        auto& workplace = em.workplaces[workplaceEntity];

        for (auto& slot : workplace.slots) {
            if (slot.workerId == static_cast<EntityID>(-1)) {
                continue;
            }

            const EntityID worker = slot.workerId;

            const bool invalidWorker = worker >= em.active.size() || !em.active[worker] || !em.hasProfession[worker] ||
                                       !em.hasTag[worker] || !em.hasBehavior[worker];

            if (invalidWorker) {
                slot.workerId = static_cast<EntityID>(-1);
                continue;
            }

            // A worker cannot occupy two slots.
            if (assignedWorkers.count(worker) > 0) {
                slot.workerId = static_cast<EntityID>(-1);
                continue;
            }

            // Ensure profession matches the slot.
            if (em.professions[worker].currentProfession != slot.profession) {
                em.professions[worker].currentProfession = slot.profession;
                ApplyBehaviorRules(worker, em, behReg);
            }

            assignedWorkers.insert(worker);
        }
    }

    // =========================================================
    // PASS 2: Release stale professions.
    //
    // If a PNJ has a profession but is not assigned to any active
    // workplace slot, reset him to "none".
    // This fixes duplicated professions after room recalculation.
    // =========================================================
    for (size_t entity = 0; entity < em.active.size(); ++entity) {
        if (!em.active[entity] || !em.hasProfession[entity] || !em.hasTag[entity] || !em.hasBehavior[entity]) {
            continue;
        }

        auto& prof = em.professions[entity];

        if (prof.currentProfession == "none") {
            continue;
        }

        if (assignedWorkers.count(entity) == 0) {
            // The entity has a profession but no active slot.
            // Keep assignmentMode unchanged:
            // - Auto remains Auto and may be reassigned later.
            // - Manual remains Manual and stays intentionally controlled by the player.
            ResetProfessionKeepMode(entity, em, behReg);
        }
    }

    // Rebuild assigned set after cleanup.
    assignedWorkers.clear();

    for (size_t workplaceEntity = 0; workplaceEntity < em.active.size(); ++workplaceEntity) {
        if (!em.active[workplaceEntity] || !em.hasWorkplace[workplaceEntity]) {
            continue;
        }

        for (const auto& slot : em.workplaces[workplaceEntity].slots) {
            if (slot.workerId != static_cast<EntityID>(-1) && slot.workerId < em.active.size() && em.active[slot.workerId]) {
                assignedWorkers.insert(slot.workerId);
            }
        }
    }

    // =========================================================
    // PASS 3: Fill empty slots.
    // =========================================================
    for (size_t workplaceEntity = 0; workplaceEntity < em.active.size(); ++workplaceEntity) {
        if (!em.active[workplaceEntity] || !em.hasWorkplace[workplaceEntity]) {
            continue;
        }

        auto& workplace = em.workplaces[workplaceEntity];

        for (auto& slot : workplace.slots) {
            if (slot.workerId != static_cast<EntityID>(-1)) {
                continue;
            }

            const ProfessionDef* profDef = profReg.GetProfession(slot.profession);

            if (!profDef) {
                continue;
            }

            for (size_t candidate = 0; candidate < em.active.size(); ++candidate) {
                if (!em.active[candidate] || !em.hasProfession[candidate] || !em.hasTag[candidate] || !em.hasBehavior[candidate]) {
                    continue;
                }

                if (assignedWorkers.count(candidate) > 0) {
                    continue;
                }

                auto& tag = em.tags[candidate];
                auto& prof = em.professions[candidate];

                if (prof.assignmentMode != ProfessionAssignmentMode::Auto) {
                    continue;
                }

                if (prof.currentProfession != "none") {
                    continue;
                }

                if (tag.age < profDef->minAge) {
                    continue;
                }

                if (tag.species != profDef->reqSpecies) {
                    continue;
                }

                slot.workerId = candidate;
                prof.currentProfession = slot.profession;
                assignedWorkers.insert(candidate);

                ApplyBehaviorRules(candidate, em, behReg);

                break;
            }
        }
    }
}
