#include "systems/ProfessionSystem.hpp"

void ProfessionSystem::Update(EntityManager& em, const ProfessionRegistry& profReg, const BehaviorRegistry& behReg) {
    // On boucle sur toutes les pièces qui offrent du travail
    for (size_t i = 0; i < em.active.size(); ++i) {
        if (!em.active[i] || !em.hasWorkplace[i])
            continue;

        auto& workplace = em.workplaces[i];

        for (auto& slot : workplace.slots) {
            // Si le poste est vacant
            if (slot.workerId == static_cast<EntityID>(-1)) {
                const ProfessionDef* profDef = profReg.GetProfession(slot.profession);
                if (!profDef)
                    continue;

                // On cherche un candidat parmi les PNJ
                for (size_t pnj = 0; pnj < em.active.size(); ++pnj) {
                    if (!em.active[pnj] || !em.hasProfession[pnj] || !em.hasTag[pnj])
                        continue;

                    auto& tag = em.tags[pnj];
                    auto& prof = em.professions[pnj];

                    // Conditions : Être au chômage, avoir le bon âge, et la bonne espèce
                    if (prof.currentProfession == "none" && tag.age >= profDef->minAge && tag.species == profDef->reqSpecies) {
                        // EMBOUCHE !
                        slot.workerId = pnj;
                        prof.currentProfession = slot.profession;

                        // MISE À JOUR DU CERVEAU (Behavior Tree)
                        auto newRules = behReg.GetBehaviorsFor(tag.category, tag.species, slot.profession);
                        em.behaviors[pnj].innateBehaviorRules = newRules;

                        em.behaviors[pnj].innateCapabilities.clear();
                        for (const auto& rule : newRules) {
                            em.behaviors[pnj].innateCapabilities.push_back(rule.name);
                        }

                        // On a trouvé un candidat, on passe au poste suivant
                        break;
                    }
                }
            }
        }
    }
}
