/**
 * @file HouseholdSystem.cpp
 * @brief Implementation of lightweight family bedroom and bed ownership.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/HouseholdSystem.hpp"

#include "core/Config.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

constexpr float HOUSEHOLD_UPDATE_INTERVAL = 2.0f;

struct RoomBedSummary {
    EntityID roomId = static_cast<EntityID>(-1);

    int totalCapacity = 0;
    int ownedByOtherFamilyCount = 0;
    int ownedBySameFamilyCount = 0;
    int unownedBedCount = 0;

    bool hasDoubleBed = false;
    bool isBedroom = false;
};

struct RoomOwnershipInference {
    bool hasFamilyBed = false;
    bool hasConflict = false;

    EntityID familyKey = static_cast<EntityID>(-1);
    EntityID villageId = static_cast<EntityID>(-1);
};

bool IsValidEntity(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity];
}

std::string GetDisplayName(const EntityManager& em, EntityID entity) {
    if (!IsValidEntity(em, entity) || !em.hasTag[entity]) {
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

int WorldToTile(float worldCoord) {
    return static_cast<int>(std::floor(worldCoord / Config::TILE_SIZE));
}

EntityID GetFamilyKey(EntityID a, EntityID b) {
    return std::min(a, b);
}

bool IsValidCouple(const EntityManager& em, EntityID a, EntityID b) {
    if (!IsValidEntity(em, a) || !IsValidEntity(em, b) || !em.hasFamily[a] || !em.hasFamily[b]) {
        return false;
    }

    if (a == b) {
        return false;
    }

    return em.families[a].partnerId == b && em.families[b].partnerId == a;
}

EntityID GetVillageIdForFamily(const EntityManager& em, EntityID a, EntityID b) {
    if (IsValidEntity(em, a) && em.hasVillageMember[a]) {
        return em.villageMembers[a].villageId;
    }

    if (IsValidEntity(em, b) && em.hasVillageMember[b]) {
        return em.villageMembers[b].villageId;
    }

    return static_cast<EntityID>(-1);
}

bool IsRoomTile(const RoomComponent& room, int tileX, int tileY) {
    for (const Vector2& tile : room.floorTiles) {
        if (static_cast<int>(tile.x) == tileX && static_cast<int>(tile.y) == tileY) {
            return true;
        }
    }

    return false;
}

bool IsEntityInsideRoom(const EntityManager& em, EntityID entity, const RoomComponent& room) {
    if (!IsValidEntity(em, entity) || !em.hasTransform[entity]) {
        return false;
    }

    const int tileX = WorldToTile(em.transforms[entity].position.x);
    const int tileY = WorldToTile(em.transforms[entity].position.y);

    return IsRoomTile(room, tileX, tileY);
}

bool IsRestSpotFinished(const EntityManager& em, EntityID entity) {
    if (!IsValidEntity(em, entity) || !em.hasRestSpot[entity]) {
        return false;
    }

    if (em.hasBlueprint[entity] && !em.blueprints[entity].isFinished) {
        return false;
    }

    return true;
}

bool IsBedroomRoom(const RoomComponent& room) {
    return room.structureId == "SMALL_BEDROOM" || room.structureId == "LARGE_BEDROOM";
}

bool IsDoubleBed(const EntityManager& em, EntityID entity) {
    if (!IsValidEntity(em, entity) || !em.hasTag[entity]) {
        return false;
    }

    return em.tags[entity].prefabId == "DOUBLE_BED";
}

std::vector<EntityID> GetRestSpotsInsideRoom(const EntityManager& em, EntityID roomId) {
    std::vector<EntityID> beds;

    if (!IsValidEntity(em, roomId) || !em.hasRoom[roomId]) {
        return beds;
    }

    const RoomComponent& room = em.rooms[roomId];

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!IsRestSpotFinished(em, entity)) {
            continue;
        }

        if (!IsEntityInsideRoom(em, entity, room)) {
            continue;
        }

        beds.push_back(entity);
    }

    return beds;
}

int CountActiveChildrenOfCouple(const EntityManager& em, EntityID parentA, EntityID parentB) {
    if (!IsValidEntity(em, parentA) || !em.hasFamily[parentA]) {
        return 0;
    }

    int count = 0;

    for (EntityID child : em.families[parentA].children) {
        if (!IsValidEntity(em, child) || !em.hasFamily[child]) {
            continue;
        }

        const FamilyComponent& family = em.families[child];

        const bool sameParents =
            (family.parentA == parentA && family.parentB == parentB) || (family.parentA == parentB && family.parentB == parentA);

        if (sameParents) {
            count++;
        }
    }

    return count;
}

int CountFamilySize(const EntityManager& em, EntityID parentA, EntityID parentB) {
    return 2 + CountActiveChildrenOfCouple(em, parentA, parentB);
}

int CountFamilyPrivateBedCapacity(const EntityManager& em, EntityID familyKey) {
    int capacity = 0;

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!IsRestSpotFinished(em, entity)) {
            continue;
        }

        const RestSpotComponent& restSpot = em.restSpots[entity];

        if (!restSpot.isPrivate) {
            continue;
        }

        if (restSpot.ownerFamilyId != familyKey) {
            continue;
        }

        capacity += std::max(0, restSpot.capacity);
    }

    return capacity;
}

bool FamilyAlreadyOwnsRoom(const EntityManager& em, EntityID familyKey) {
    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!IsValidEntity(em, entity) || !em.hasRoom[entity]) {
            continue;
        }

        if (em.rooms[entity].ownerFamilyId == familyKey) {
            return true;
        }
    }

    return false;
}

RoomBedSummary AnalyzeRoomBeds(const EntityManager& em, EntityID roomId, EntityID familyKey) {
    RoomBedSummary summary;
    summary.roomId = roomId;

    if (!IsValidEntity(em, roomId) || !em.hasRoom[roomId]) {
        return summary;
    }

    const RoomComponent& room = em.rooms[roomId];
    summary.isBedroom = IsBedroomRoom(room);

    const std::vector<EntityID> beds = GetRestSpotsInsideRoom(em, roomId);

    for (EntityID bed : beds) {
        const RestSpotComponent& restSpot = em.restSpots[bed];

        summary.totalCapacity += std::max(0, restSpot.capacity);

        if (IsDoubleBed(em, bed)) {
            summary.hasDoubleBed = true;
        }

        if (restSpot.ownerFamilyId == familyKey) {
            summary.ownedBySameFamilyCount++;
            continue;
        }

        if (restSpot.ownerFamilyId != static_cast<EntityID>(-1) && restSpot.ownerFamilyId != familyKey) {
            summary.ownedByOtherFamilyCount++;
            continue;
        }

        summary.unownedBedCount++;
    }

    return summary;
}

RoomOwnershipInference InferRoomOwnershipFromFamilyBeds(const EntityManager& em, EntityID roomId) {
    RoomOwnershipInference inference;

    if (!IsValidEntity(em, roomId) || !em.hasRoom[roomId]) {
        return inference;
    }

    const std::vector<EntityID> beds = GetRestSpotsInsideRoom(em, roomId);

    for (EntityID bed : beds) {
        const RestSpotComponent& restSpot = em.restSpots[bed];

        if (restSpot.ownerFamilyId == static_cast<EntityID>(-1)) {
            continue;
        }

        if (!inference.hasFamilyBed) {
            inference.hasFamilyBed = true;
            inference.familyKey = restSpot.ownerFamilyId;
            inference.villageId = restSpot.ownerVillageId;
            continue;
        }

        if (inference.familyKey != restSpot.ownerFamilyId) {
            inference.hasConflict = true;
            return inference;
        }
    }

    return inference;
}

/**
 * @brief Restores room ownership after RoomSystem destroys/recreates room entities.
 *
 * Beds are persistent entities, while RoomComponent entities are transient.
 * Therefore, if a newly detected bedroom contains a family-owned bed,
 * the room ownership is restored from the bed ownership.
 */
void RestoreRoomOwnershipFromFamilyBeds(EntityManager& em) {
    for (EntityID roomId = 0; roomId < em.active.size(); ++roomId) {
        if (!IsValidEntity(em, roomId) || !em.hasRoom[roomId]) {
            continue;
        }

        RoomComponent& room = em.rooms[roomId];

        if (!IsBedroomRoom(room)) {
            continue;
        }

        const RoomOwnershipInference inference = InferRoomOwnershipFromFamilyBeds(em, roomId);

        if (!inference.hasFamilyBed) {
            continue;
        }

        if (inference.hasConflict) {
            std::cout << "[HOUSEHOLD] Warning: bedroom " << room.name
                      << " contains beds owned by multiple families. Room ownership not restored." << std::endl;
            continue;
        }

        const bool changed = !room.isPrivate || room.ownerFamilyId != inference.familyKey || room.ownerVillageId != inference.villageId;

        room.isPrivate = true;
        room.ownerFamilyId = inference.familyKey;
        room.ownerVillageId = inference.villageId;

        if (changed) {
            std::cout << "[HOUSEHOLD] Restored " << room.name << " ownership for family " << inference.familyKey << "." << std::endl;
        }
    }
}

bool IsAssignableRoomForFamily(const EntityManager& em, EntityID roomId, EntityID familyKey) {
    if (!IsValidEntity(em, roomId) || !em.hasRoom[roomId]) {
        return false;
    }

    const RoomComponent& room = em.rooms[roomId];

    if (!IsBedroomRoom(room)) {
        return false;
    }

    if (room.ownerFamilyId != static_cast<EntityID>(-1) && room.ownerFamilyId != familyKey) {
        return false;
    }

    const RoomBedSummary summary = AnalyzeRoomBeds(em, roomId, familyKey);

    if (summary.ownedByOtherFamilyCount > 0) {
        return false;
    }

    if (summary.totalCapacity <= 0) {
        return false;
    }

    // V1: a couple should receive a bedroom containing a double bed.
    // This prevents assigning a one-person small bedroom to a couple.
    if (!summary.hasDoubleBed) {
        return false;
    }

    return true;
}

EntityID FindBestAssignableBedroomForFamily(const EntityManager& em, EntityID familyKey, int desiredCapacity) {
    EntityID bestRoom = static_cast<EntityID>(-1);
    int bestScore = std::numeric_limits<int>::min();

    for (EntityID roomId = 0; roomId < em.active.size(); ++roomId) {
        if (!IsAssignableRoomForFamily(em, roomId, familyKey)) {
            continue;
        }

        const RoomBedSummary summary = AnalyzeRoomBeds(em, roomId, familyKey);

        int score = 0;

        if (summary.ownedBySameFamilyCount > 0) {
            score += 2000;
        }

        if (summary.hasDoubleBed) {
            score += 1000;
        }

        if (summary.totalCapacity >= desiredCapacity) {
            score += 500;
        }

        score += summary.totalCapacity * 10;
        score += summary.unownedBedCount;

        if (score > bestScore) {
            bestScore = score;
            bestRoom = roomId;
        }
    }

    return bestRoom;
}

void AssignBedroomToFamily(EntityManager& em, EntityID roomId, EntityID familyKey, EntityID ownerVillageId) {
    if (!IsValidEntity(em, roomId) || !em.hasRoom[roomId]) {
        return;
    }

    RoomComponent& room = em.rooms[roomId];

    room.isPrivate = true;
    room.ownerFamilyId = familyKey;
    room.ownerVillageId = ownerVillageId;

    const std::vector<EntityID> beds = GetRestSpotsInsideRoom(em, roomId);

    for (EntityID bed : beds) {
        RestSpotComponent& restSpot = em.restSpots[bed];

        // Do not steal beds already owned by another family.
        if (restSpot.ownerFamilyId != static_cast<EntityID>(-1) && restSpot.ownerFamilyId != familyKey) {
            continue;
        }

        restSpot.isPrivate = true;
        restSpot.ownerFamilyId = familyKey;
        restSpot.ownerVillageId = ownerVillageId;
    }
}

void EnsureCoupleHasBedroom(EntityManager& em, EntityID parentA, EntityID parentB) {
    if (!IsValidCouple(em, parentA, parentB)) {
        return;
    }

    const EntityID familyKey = GetFamilyKey(parentA, parentB);
    const EntityID villageId = GetVillageIdForFamily(em, parentA, parentB);

    if (villageId == static_cast<EntityID>(-1)) {
        return;
    }

    const int familySize = CountFamilySize(em, parentA, parentB);

    // V1 target:
    // family size + 1 means the couple can have one child if the room supports it.
    const int desiredCapacity = familySize + 1;

    const int currentCapacity = CountFamilyPrivateBedCapacity(em, familyKey);

    if (currentCapacity >= desiredCapacity && FamilyAlreadyOwnsRoom(em, familyKey)) {
        return;
    }

    const EntityID roomId = FindBestAssignableBedroomForFamily(em, familyKey, desiredCapacity);

    if (roomId == static_cast<EntityID>(-1)) {
        return;
    }

    AssignBedroomToFamily(em, roomId, familyKey, villageId);

    std::cout << "[HOUSEHOLD] Assigned " << em.rooms[roomId].name << " to family " << familyKey << " (" << GetDisplayName(em, parentA)
              << " + " << GetDisplayName(em, parentB) << ")." << std::endl;
}

} // namespace

void HouseholdSystem::Update(float deltaTime, EntityManager& em) {
    m_updateAccumulator += deltaTime;

    if (m_updateAccumulator < HOUSEHOLD_UPDATE_INTERVAL) {
        return;
    }

    m_updateAccumulator = 0.0f;

    // Room entities are recreated by RoomSystem.
    // Restore room ownership from persistent bed ownership first.
    RestoreRoomOwnershipFromFamilyBeds(em);

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!IsValidEntity(em, entity) || !em.hasFamily[entity]) {
            continue;
        }

        const EntityID partner = em.families[entity].partnerId;

        if (partner == static_cast<EntityID>(-1)) {
            continue;
        }

        if (!IsValidEntity(em, partner) || !em.hasFamily[partner]) {
            continue;
        }

        // Avoid processing the same couple twice.
        if (partner < entity) {
            continue;
        }

        EnsureCoupleHasBedroom(em, entity, partner);
    }
}
