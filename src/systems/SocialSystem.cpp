/**
 * @file SocialSystem.cpp
 * @brief Implementation of lightweight social relationship simulation.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/SocialSystem.hpp"

#include "core/Config.hpp"

#include <algorithm>
#include <iostream>
#include <string>

namespace {

std::string GetDisplayName(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasTag[entity]) {
        return "Unknown";
    }

    const TagComponent& tag = em.tags[entity];

    if (!tag.firstName.empty()) {
        return tag.firstName;
    }

    if (!tag.name.empty()) {
        return tag.name;
    }

    return "Entity #" + std::to_string(entity);
}

bool IsValidSocialHuman(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasTag[entity] || !em.hasTransform[entity] ||
        !em.hasVillageMember[entity] || !em.hasSocial[entity]) {
        return false;
    }

    return em.tags[entity].species == "human";
}

bool AreSameVillage(const EntityManager& em, EntityID a, EntityID b) {
    if (!em.hasVillageMember[a] || !em.hasVillageMember[b]) {
        return false;
    }

    return em.villageMembers[a].villageId == em.villageMembers[b].villageId;
}

bool IsAdult(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasTag[entity] && em.tags[entity].age >= Config::ADULT_AGE;
}

bool HasPartner(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasFamily[entity]) {
        return false;
    }

    return em.families[entity].partnerId != static_cast<EntityID>(-1);
}

bool AreRomanceCompatible(const EntityManager& em, EntityID a, EntityID b) {
    if (!IsAdult(em, a) || !IsAdult(em, b)) {
        return false;
    }

    if (HasPartner(em, a) || HasPartner(em, b)) {
        return false;
    }

    const std::string& genderA = em.tags[a].gender;
    const std::string& genderB = em.tags[b].gender;

    if (genderA == "undefined" || genderB == "undefined") {
        return false;
    }

    // V1 keeps the current binary model simple.
    // This can be replaced later by data-driven attraction rules.
    return genderA != genderB;
}

void EnsureFamilyComponent(EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity]) {
        return;
    }

    if (!em.hasFamily[entity]) {
        em.hasFamily[entity] = true;
        em.families[entity] = {};
    }
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

void MakePartners(EntityManager& em, EntityID a, EntityID b) {
    EnsureFamilyComponent(em, a);
    EnsureFamilyComponent(em, b);

    if (em.families[a].partnerId != static_cast<EntityID>(-1) || em.families[b].partnerId != static_cast<EntityID>(-1)) {
        return;
    }

    em.families[a].partnerId = b;
    em.families[b].partnerId = a;

    std::cout << "[SOCIAL] " << GetDisplayName(em, a) << " and " << GetDisplayName(em, b) << " became partners." << std::endl;
}
float ClampSocial(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }

    if (value > 100.0f) {
        return 100.0f;
    }

    return value;
}

float GetSociabilityMultiplier(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasPersonality[entity]) {
        return 1.0f;
    }

    return 0.75f + em.personalities[entity].sociability * 0.5f;
}

float GetResentmentDecayMultiplier(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasPersonality[entity]) {
        return 1.0f;
    }

    return em.personalities[entity].resentmentDecayMultiplier;
}

bool IsHatred(const RelationshipEntry& relationship) {
    return relationship.resentment >= 70.0f && relationship.friendship <= 20.0f;
}

void UpdateRelationshipPair(EntityManager& em, EntityID a, EntityID b) {
    RelationshipEntry& relAB = GetOrCreateRelationship(em, a, b);
    RelationshipEntry& relBA = GetOrCreateRelationship(em, b, a);

    const float gainAB = Config::FRIENDSHIP_GAIN_PER_UPDATE * GetSociabilityMultiplier(em, a);
    const float gainBA = Config::FRIENDSHIP_GAIN_PER_UPDATE * GetSociabilityMultiplier(em, b);

    relAB.friendship = ClampSocial(relAB.friendship + gainAB);
    relBA.friendship = ClampSocial(relBA.friendship + gainBA);

    // Nearby peaceful contact slowly reduces resentment.
    // Vengeful traits reduce this decay via resentmentDecayMultiplier.
    relAB.resentment = ClampSocial(relAB.resentment - 0.25f * GetResentmentDecayMultiplier(em, a));
    relBA.resentment = ClampSocial(relBA.resentment - 0.25f * GetResentmentDecayMultiplier(em, b));

    if (relAB.friendship >= Config::FRIENDSHIP_THRESHOLD && !relAB.friendshipAnnounced) {
        relAB.friendshipAnnounced = true;
        relBA.friendshipAnnounced = true;

        std::cout << "[SOCIAL] " << GetDisplayName(em, a) << " and " << GetDisplayName(em, b) << " became friends." << std::endl;
    }

    if (IsHatred(relAB) && !relAB.hatredAnnounced) {
        relAB.hatredAnnounced = true;

        std::cout << "[SOCIAL] " << GetDisplayName(em, a) << " now hates " << GetDisplayName(em, b) << "." << std::endl;
    }

    if (IsHatred(relBA) && !relBA.hatredAnnounced) {
        relBA.hatredAnnounced = true;

        std::cout << "[SOCIAL] " << GetDisplayName(em, b) << " now hates " << GetDisplayName(em, a) << "." << std::endl;
    }

    if (!AreRomanceCompatible(em, a, b)) {
        return;
    }

    if (relAB.friendship < Config::FRIENDSHIP_THRESHOLD || relBA.friendship < Config::FRIENDSHIP_THRESHOLD) {
        return;
    }

    relAB.romance = std::min(100.0f, relAB.romance + Config::ROMANCE_GAIN_PER_UPDATE);
    relBA.romance = std::min(100.0f, relBA.romance + Config::ROMANCE_GAIN_PER_UPDATE);

    if (relAB.romance >= Config::ROMANCE_THRESHOLD && !relAB.romanceAnnounced) {
        relAB.romanceAnnounced = true;
        relBA.romanceAnnounced = true;

        MakePartners(em, a, b);
    }
}

} // namespace

void SocialSystem::Update(float deltaTime, EntityManager& em, const EntitySpatialGrid& spatialGrid) {
    m_updateAccumulator += deltaTime;

    if (m_updateAccumulator < Config::SOCIAL_UPDATE_INTERVAL) {
        return;
    }

    m_updateAccumulator = 0.0f;

    const float radiusWorld = Config::SOCIAL_RADIUS_TILES * Config::TILE_SIZE;

    for (EntityID a = 0; a < em.active.size(); ++a) {
        if (!IsValidSocialHuman(em, a)) {
            continue;
        }

        const std::vector<EntityID> nearby = spatialGrid.GetEntitiesInRadius(em.transforms[a].position, radiusWorld, em);

        for (EntityID b : nearby) {
            if (b <= a) {
                continue;
            }

            if (!IsValidSocialHuman(em, b)) {
                continue;
            }

            if (!AreSameVillage(em, a, b)) {
                continue;
            }

            UpdateRelationshipPair(em, a, b);
        }
    }
}
