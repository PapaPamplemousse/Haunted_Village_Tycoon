/**
 * @file HouseholdSystem.cpp
 * @brief Implementation of lightweight household ownership logic.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/HouseholdSystem.hpp"

#include <algorithm>
#include <iostream>
#include <string>

namespace {

constexpr float HOUSEHOLD_UPDATE_INTERVAL = 1.0f;

EntityID GetFamilyKey(EntityID a, EntityID b) {
    return std::min(a, b);
}

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

bool IsValidPartnerPair(const EntityManager& em, EntityID a, EntityID b) {
    if (a >= em.active.size() || b >= em.active.size() || !em.active[a] || !em.active[b] || !em.hasFamily[a] || !em.hasFamily[b] ||
        !em.hasVillageMember[a] || !em.hasVillageMember[b]) {
        return false;
    }

    if (em.families[a].partnerId != b || em.families[b].partnerId != a) {
        return false;
    }

    return em.villageMembers[a].villageId == em.villageMembers[b].villageId;
}

bool IsCompletedRestSpot(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasRestSpot[entity]) {
        return false;
    }

    if (em.hasBlueprint[entity] && !em.blueprints[entity].isFinished) {
        return false;
    }

    return true;
}

int CountPrivateBedCapacityForFamily(const EntityManager& em, EntityID familyKey) {
    int capacity = 0;

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!IsCompletedRestSpot(em, entity)) {
            continue;
        }

        const RestSpotComponent& restSpot = em.restSpots[entity];

        if (restSpot.isPrivate && restSpot.ownerFamilyId == familyKey) {
            capacity += std::max(0, restSpot.capacity);
        }
    }

    return capacity;
}

int CountFamilySize(const EntityManager& em, EntityID parentA, EntityID parentB) {
    int size = 2;

    if (parentA < em.active.size() && em.active[parentA] && em.hasFamily[parentA]) {
        for (EntityID child : em.families[parentA].children) {
            if (child < em.active.size() && em.active[child]) {
                size++;
            }
        }
    }

    return size;
}

EntityID FindBestPublicBedForVillage(const EntityManager& em, EntityID villageId) {
    EntityID bestBed = static_cast<EntityID>(-1);
    int bestCapacity = -1;

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!IsCompletedRestSpot(em, entity)) {
            continue;
        }

        const RestSpotComponent& restSpot = em.restSpots[entity];

        if (restSpot.isPrivate) {
            continue;
        }

        if (restSpot.ownerVillageId != static_cast<EntityID>(-1) && restSpot.ownerVillageId != villageId) {
            continue;
        }

        if (restSpot.capacity > bestCapacity) {
            bestCapacity = restSpot.capacity;
            bestBed = entity;
        }
    }

    return bestBed;
}

void AssignBedToFamily(EntityManager& em, EntityID bed, EntityID villageId, EntityID familyKey, EntityID partnerA, EntityID partnerB) {
    if (!IsCompletedRestSpot(em, bed)) {
        return;
    }

    RestSpotComponent& restSpot = em.restSpots[bed];

    restSpot.isPrivate = true;
    restSpot.ownerVillageId = villageId;
    restSpot.ownerFamilyId = familyKey;

    std::cout << "[HOUSEHOLD] Assigned bed #" << bed << " to family #" << familyKey << " (" << GetDisplayName(em, partnerA) << " + "
              << GetDisplayName(em, partnerB) << ")." << std::endl;
}

} // namespace

void HouseholdSystem::Update(float deltaTime, EntityManager& em) {
    m_updateAccumulator += deltaTime;

    if (m_updateAccumulator < HOUSEHOLD_UPDATE_INTERVAL) {
        return;
    }

    m_updateAccumulator = 0.0f;

    for (EntityID a = 0; a < em.active.size(); ++a) {
        if (a >= em.active.size() || !em.active[a] || !em.hasFamily[a]) {
            continue;
        }

        const EntityID b = em.families[a].partnerId;

        if (b == static_cast<EntityID>(-1)) {
            continue;
        }

        // Avoid processing the same couple twice.
        if (b < a) {
            continue;
        }

        if (!IsValidPartnerPair(em, a, b)) {
            continue;
        }

        const EntityID villageId = em.villageMembers[a].villageId;
        const EntityID familyKey = GetFamilyKey(a, b);

        const int requiredCapacity = CountFamilySize(em, a, b) + 1;

        while (CountPrivateBedCapacityForFamily(em, familyKey) < requiredCapacity) {
            const EntityID publicBed = FindBestPublicBedForVillage(em, villageId);

            if (publicBed == static_cast<EntityID>(-1)) {
                break;
            }

            AssignBedToFamily(em, publicBed, villageId, familyKey, a, b);
        }
    }
}
