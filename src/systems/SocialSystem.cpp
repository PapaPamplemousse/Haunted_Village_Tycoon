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

constexpr float SOCIAL_UPDATE_INTERVAL = 1.0f;

// constexpr float SOCIAL_RADIUS_TILES = 4.0f;

// constexpr float FRIENDSHIP_GAIN_PER_UPDATE = 1.0f;
// constexpr float ROMANCE_GAIN_PER_UPDATE = 0.35f;

constexpr float SOCIAL_RADIUS_TILES = 40.0f;

constexpr float FRIENDSHIP_GAIN_PER_UPDATE = 4.0f;
constexpr float ROMANCE_GAIN_PER_UPDATE = 2.0f;

constexpr float FRIENDSHIP_THRESHOLD = 50.0f;
constexpr float ROMANCE_THRESHOLD = 70.0f;

constexpr int ADULT_AGE = 16;

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
    return entity < em.active.size() && em.active[entity] && em.hasTag[entity] && em.tags[entity].age >= ADULT_AGE;
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

void UpdateRelationshipPair(EntityManager& em, EntityID a, EntityID b) {
    RelationshipEntry& relAB = GetOrCreateRelationship(em, a, b);
    RelationshipEntry& relBA = GetOrCreateRelationship(em, b, a);

    relAB.friendship = std::min(100.0f, relAB.friendship + FRIENDSHIP_GAIN_PER_UPDATE);
    relBA.friendship = std::min(100.0f, relBA.friendship + FRIENDSHIP_GAIN_PER_UPDATE);

    if (relAB.friendship >= FRIENDSHIP_THRESHOLD && !relAB.friendshipAnnounced) {
        relAB.friendshipAnnounced = true;
        relBA.friendshipAnnounced = true;

        std::cout << "[SOCIAL] " << GetDisplayName(em, a) << " and " << GetDisplayName(em, b) << " became friends." << std::endl;
    }

    if (!AreRomanceCompatible(em, a, b)) {
        return;
    }

    if (relAB.friendship < FRIENDSHIP_THRESHOLD || relBA.friendship < FRIENDSHIP_THRESHOLD) {
        return;
    }

    relAB.romance = std::min(100.0f, relAB.romance + ROMANCE_GAIN_PER_UPDATE);
    relBA.romance = std::min(100.0f, relBA.romance + ROMANCE_GAIN_PER_UPDATE);

    if (relAB.romance >= ROMANCE_THRESHOLD && !relAB.romanceAnnounced) {
        relAB.romanceAnnounced = true;
        relBA.romanceAnnounced = true;

        MakePartners(em, a, b);
    }
}

} // namespace

void SocialSystem::Update(float deltaTime, EntityManager& em, const EntitySpatialGrid& spatialGrid) {
    m_updateAccumulator += deltaTime;

    if (m_updateAccumulator < SOCIAL_UPDATE_INTERVAL) {
        return;
    }

    m_updateAccumulator = 0.0f;

    const float radiusWorld = SOCIAL_RADIUS_TILES * Config::TILE_SIZE;

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
