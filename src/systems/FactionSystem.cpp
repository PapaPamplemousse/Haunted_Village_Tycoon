/**
 * @file FactionSystem.cpp
 * @brief Implementation of faction/religion simulation.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/FactionSystem.hpp"

#include <algorithm>
#include <iostream>

namespace {

constexpr float FACTION_UPDATE_INTERVAL = 5.0f;

bool IsHuman(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasTag[entity] && em.tags[entity].species == "human";
}

float Clamp100(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }

    if (value > 100.0f) {
        return 100.0f;
    }

    return value;
}

bool HasTrait(const EntityManager& em, EntityID entity, const std::string& traitId) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasPersonality[entity]) {
        return false;
    }

    const std::vector<std::string>& traits = em.personalities[entity].traits;

    return std::find(traits.begin(), traits.end(), traitId) != traits.end();
}

bool AreOpposedFactions(const FactionDef& faction, const std::string& otherFactionId) {
    return std::find(faction.opposedFactions.begin(), faction.opposedFactions.end(), otherFactionId) != faction.opposedFactions.end();
}

RelationshipEntry& GetOrCreateRelationship(EntityManager& em, EntityID owner, EntityID other) {
    SocialComponent& social = em.socials[owner];

    for (RelationshipEntry& relationship : social.relationships) {
        if (relationship.otherId == other) {
            return relationship;
        }
    }

    social.relationships.push_back({other});
    return social.relationships.back();
}

void ApplyFactionSocialDrift(EntityManager& em, EntityID a, EntityID b, const FactionRegistry& factionReg) {
    if (!em.hasFaction[a] || !em.hasFaction[b] || !em.hasSocial[a] || !em.hasSocial[b]) {
        return;
    }

    const FactionComponent& factionA = em.factions[a];
    const FactionComponent& factionB = em.factions[b];

    const FactionDef* defA = factionReg.GetFaction(factionA.factionId);
    const FactionDef* defB = factionReg.GetFaction(factionB.factionId);

    if (defA == nullptr || defB == nullptr) {
        return;
    }

    RelationshipEntry& relAB = GetOrCreateRelationship(em, a, b);
    RelationshipEntry& relBA = GetOrCreateRelationship(em, b, a);

    const float convictionFactorA = factionA.conviction / 100.0f;
    const float convictionFactorB = factionB.conviction / 100.0f;

    if (factionA.factionId == factionB.factionId) {
        relAB.trust = Clamp100(relAB.trust + defA->sameFactionTrustGain * convictionFactorA);
        relBA.trust = Clamp100(relBA.trust + defB->sameFactionTrustGain * convictionFactorB);
        return;
    }

    if (AreOpposedFactions(*defA, factionB.factionId)) {
        relAB.resentment = Clamp100(relAB.resentment + defA->opposedFactionResentmentGain * convictionFactorA);
        relAB.trust = Clamp100(relAB.trust - 0.05f * convictionFactorA);
    }

    if (AreOpposedFactions(*defB, factionA.factionId)) {
        relBA.resentment = Clamp100(relBA.resentment + defB->opposedFactionResentmentGain * convictionFactorB);
        relBA.trust = Clamp100(relBA.trust - 0.05f * convictionFactorB);
    }
}

std::string ChooseFactionForHuman(const EntityManager& em, EntityID entity, const FactionRegistry& factionReg,
                                  const SettlementMetrics& metrics) {
    // V1 deterministic-ish weighted tendency.
    // Common folk remains default unless fear/corruption or traits push elsewhere.

    float oldFaithScore = 10.0f + metrics.fear * 0.25f - metrics.corruption * 0.10f;
    float ironWatchScore = 10.0f + metrics.fear * 0.18f;
    float cultScore = 2.0f + metrics.corruption * 0.45f + metrics.fear * 0.08f;
    float commonScore = 25.0f;

    if (HasTrait(em, entity, "BRAVE")) {
        ironWatchScore += 10.0f;
    }

    if (HasTrait(em, entity, "COWARD")) {
        oldFaithScore += 8.0f;
    }

    if (HasTrait(em, entity, "VIOLENT") || HasTrait(em, entity, "VENGEFUL")) {
        cultScore += 6.0f;
        ironWatchScore += 4.0f;
    }

    if (HasTrait(em, entity, "KIND") || HasTrait(em, entity, "LOYAL")) {
        oldFaithScore += 4.0f;
        commonScore += 5.0f;
    }

    if (cultScore > oldFaithScore && cultScore > ironWatchScore && cultScore > commonScore) {
        return factionReg.GetFaction("CULT_OF_THE_HOLLOW") != nullptr ? "CULT_OF_THE_HOLLOW" : "COMMON_FOLK";
    }

    if (ironWatchScore > oldFaithScore && ironWatchScore > commonScore) {
        return factionReg.GetFaction("IRON_WATCH") != nullptr ? "IRON_WATCH" : "COMMON_FOLK";
    }

    if (oldFaithScore > commonScore) {
        return factionReg.GetFaction("OLD_FAITH") != nullptr ? "OLD_FAITH" : "COMMON_FOLK";
    }

    return "COMMON_FOLK";
}

} // namespace

void FactionSystem::AssignMissingFactions(EntityManager& em, const FactionRegistry& factionReg) {
    const FactionDef* common = factionReg.GetFaction("COMMON_FOLK");

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!IsHuman(em, entity)) {
            continue;
        }

        if (em.hasFaction[entity]) {
            continue;
        }

        em.hasFaction[entity] = true;
        em.factions[entity] = {};

        if (common != nullptr) {
            em.factions[entity].factionId = common->id;
            em.factions[entity].conviction = common->baseConviction;
        }
    }
}

void FactionSystem::Update(float deltaTime, EntityManager& em, const FactionRegistry& factionReg, const SettlementMetrics& metrics) {
    m_updateAccumulator += deltaTime;

    if (m_updateAccumulator < FACTION_UPDATE_INTERVAL) {
        return;
    }

    m_updateAccumulator = 0.0f;

    AssignMissingFactions(em, factionReg);

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!IsHuman(em, entity) || !em.hasFaction[entity]) {
            continue;
        }

        FactionComponent& faction = em.factions[entity];

        const FactionDef* currentDef = factionReg.GetFaction(faction.factionId);

        if (currentDef == nullptr) {
            faction.factionId = "COMMON_FOLK";
            faction.conviction = 10.0f;
            continue;
        }

        // Fear/corruption strengthen or weaken current conviction.
        const float pressure =
            metrics.fear * currentDef->fearAffinity * 0.01f + metrics.corruption * currentDef->corruptionAffinity * 0.01f;

        faction.conviction = Clamp100(faction.conviction + pressure);

        // Very low conviction allows soft faction drift.
        if (faction.conviction < 8.0f || faction.factionId == "COMMON_FOLK") {
            const std::string candidateFaction = ChooseFactionForHuman(em, entity, factionReg, metrics);

            if (candidateFaction != faction.factionId) {
                const FactionDef* newDef = factionReg.GetFaction(candidateFaction);

                if (newDef != nullptr) {
                    faction.factionId = newDef->id;
                    faction.conviction = newDef->baseConviction;

                    std::cout << "[FACTION] Entity #" << entity << " joined " << newDef->name << "." << std::endl;
                }
            }
        }
    }

    // Social drift between nearby/known villagers.
    for (EntityID a = 0; a < em.active.size(); ++a) {
        if (!IsHuman(em, a) || !em.hasVillageMember[a] || !em.hasSocial[a] || !em.hasFaction[a]) {
            continue;
        }

        for (const RelationshipEntry& relationship : em.socials[a].relationships) {
            const EntityID b = relationship.otherId;

            if (!IsHuman(em, b) || !em.hasVillageMember[b] || !em.hasFaction[b]) {
                continue;
            }

            if (em.villageMembers[a].villageId != em.villageMembers[b].villageId) {
                continue;
            }

            ApplyFactionSocialDrift(em, a, b, factionReg);
        }
    }
}
