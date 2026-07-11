#include "systems/ProfessionSystem.hpp"

#include "core/Config.hpp"

void ProfessionSystem::Update(float deltaTime, EntityManager& em, const ProfessionRegistry& profReg, const BehaviorRegistry& behReg) {
    m_updateAccumulator += deltaTime;

    if (m_updateAccumulator < Config::PROFESSION_UPDATE_INTERVAL) {
        return;
    }

    m_updateAccumulator = 0.0f;

    for (size_t i = 0; i < em.active.size(); ++i) {
        if (!em.active[i] || !em.hasWorkplace[i]) {
            continue;
        }

        auto& workplace = em.workplaces[i];

        for (auto& slot : workplace.slots) {
            // Clean invalid/dead worker.
            if (slot.workerId != static_cast<EntityID>(-1)) {
                if (slot.workerId >= em.active.size() || !em.active[slot.workerId]) {
                    slot.workerId = static_cast<EntityID>(-1);
                }
            }

            if (slot.workerId != static_cast<EntityID>(-1)) {
                continue;
            }

            const ProfessionDef* profDef = profReg.GetProfession(slot.profession);

            if (!profDef) {
                continue;
            }

            for (size_t pnj = 0; pnj < em.active.size(); ++pnj) {
                if (!em.active[pnj] || !em.hasProfession[pnj] || !em.hasTag[pnj] || !em.hasBehavior[pnj]) {
                    continue;
                }

                auto& tag = em.tags[pnj];
                auto& prof = em.professions[pnj];

                if (prof.currentProfession != "none") {
                    continue;
                }

                if (tag.age < profDef->minAge) {
                    continue;
                }

                if (tag.species != profDef->reqSpecies) {
                    continue;
                }

                slot.workerId = pnj;
                prof.currentProfession = slot.profession;

                auto newRules = behReg.GetBehaviorsFor(tag.category, tag.species, slot.profession);

                em.behaviors[pnj].innateBehaviorRules = newRules;
                em.behaviors[pnj].innateCapabilities.clear();

                for (const auto& rule : newRules) {
                    em.behaviors[pnj].innateCapabilities.push_back(rule.name);
                }

                break;
            }
        }
    }
}
