/**
 * @file VillageSystem.cpp
 * @brief Implementation of village and population management logic.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/VillageSystem.hpp"

#include "core/Config.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
// forward declaratoip,
bool HasFamilyHousingCapacityForNewChild(const EntityManager& em, EntityID parentA, EntityID parentB);

constexpr int DEFAULT_VILLAGE_POPULATION_LIMIT = 10;
constexpr float BIRTH_FOOD_NUTRITION_COST = 80.0f;

int WorldToTile(float value) {
    return static_cast<int>(std::floor(value / Config::TILE_SIZE));
}

Vector2 TileToWorldCenter(int tileX, int tileY) {
    return {tileX * Config::TILE_SIZE + Config::TILE_SIZE * 0.5f, tileY * Config::TILE_SIZE + Config::TILE_SIZE * 0.5f};
}

bool IsTileWalkable(const WorldMap& map, const TileRegistry& tileReg, int tileX, int tileY) {
    const int tileId = map.GetTile(tileX, tileY);
    const TileDef* tileDef = tileReg.GetTileDef(tileId);

    return tileDef != nullptr && tileDef->walkable;
}

bool IsTileOccupied(const EntityManager& em, int tileX, int tileY) {
    for (EntityID id = 0; id < em.active.size(); ++id) {
        if (!em.active[id] || !em.hasTransform[id]) {
            continue;
        }

        const int ex = WorldToTile(em.transforms[id].position.x);
        const int ey = WorldToTile(em.transforms[id].position.y);

        if (ex == tileX && ey == tileY) {
            return true;
        }
    }

    return false;
}

bool IsValidSpawnTile(const EntityManager& em, const WorldMap& map, const TileRegistry& tileReg, int tileX, int tileY) {
    if (tileX < 0 || tileY < 0 || tileX >= map.GetWidth() || tileY >= map.GetHeight()) {
        return false;
    }

    if (!IsTileWalkable(map, tileReg, tileX, tileY)) {
        return false;
    }

    if (IsTileOccupied(em, tileX, tileY)) {
        return false;
    }

    return true;
}

Vector2 FindValidWorldPositionNear(const EntityManager& em, const WorldMap& map, const TileRegistry& tileReg, Vector2 preferredCenter,
                                   int maxRadiusTiles) {
    const int centerX = WorldToTile(preferredCenter.x);
    const int centerY = WorldToTile(preferredCenter.y);

    for (int radius = 0; radius <= maxRadiusTiles; ++radius) {
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                if (std::abs(dx) != radius && std::abs(dy) != radius) {
                    continue;
                }

                const int tileX = centerX + dx;
                const int tileY = centerY + dy;

                if (IsValidSpawnTile(em, map, tileReg, tileX, tileY)) {
                    return TileToWorldCenter(tileX, tileY);
                }
            }
        }
    }

    return preferredCenter;
}

Vector2 FindVillagerSpawnPosition(const EntityManager& em, const WorldMap& map, const TileRegistry& tileReg, Vector2 villageCenter,
                                  int index) {
    static const int OFFSETS[][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {1, -1},
                                     {-1, 1}, {1, 1}, {-2, 0}, {2, 0}, {0, -2},  {0, 2}};

    const int centerX = WorldToTile(villageCenter.x);
    const int centerY = WorldToTile(villageCenter.y);

    const int offsetCount = static_cast<int>(sizeof(OFFSETS) / sizeof(OFFSETS[0]));

    for (int attempt = 0; attempt < offsetCount; ++attempt) {
        const int offsetIndex = (index + attempt) % offsetCount;

        const int tileX = centerX + OFFSETS[offsetIndex][0];
        const int tileY = centerY + OFFSETS[offsetIndex][1];

        if (IsValidSpawnTile(em, map, tileReg, tileX, tileY)) {
            return TileToWorldCenter(tileX, tileY);
        }
    }

    return FindValidWorldPositionNear(em, map, tileReg, villageCenter, 8);
}

void AddStartingResources(EntityManager& em, EntityID villageCore) {
    if (villageCore >= em.active.size() || !em.active[villageCore] || !em.hasInventory[villageCore]) {
        return;
    }

    auto& inventory = em.inventories[villageCore];

    inventory.items["WOOD"] += 80;
    inventory.items["STONE"] += 20;
    inventory.items["ROPE"] += 10;
    inventory.items["BUSH_BERRY"] += 10;
}

float ComputeFoodNutritionInInventory(const InventoryComponent& inventory, const ResourceRegistry& resourceReg) {
    float totalNutrition = 0.0f;

    for (const auto& item : inventory.items) {
        const std::string& itemId = item.first;
        const int count = item.second;

        if (count <= 0) {
            continue;
        }

        const ResourceDef* resource = resourceReg.GetResourceDef(itemId);

        if (resource == nullptr || !resource->isConsumable || resource->nutrition <= 0.0f) {
            continue;
        }

        totalNutrition += resource->nutrition * static_cast<float>(count);
    }

    return totalNutrition;
}

bool ConsumeFoodNutritionFromInventory(InventoryComponent& inventory, const ResourceRegistry& resourceReg, float nutritionCost) {
    if (ComputeFoodNutritionInInventory(inventory, resourceReg) < nutritionCost) {
        return false;
    }

    float remainingNutrition = nutritionCost;
    std::vector<std::string> emptyItems;

    for (auto& item : inventory.items) {
        if (remainingNutrition <= 0.0f) {
            break;
        }

        const std::string& itemId = item.first;
        int& count = item.second;

        if (count <= 0) {
            emptyItems.push_back(itemId);
            continue;
        }

        const ResourceDef* resource = resourceReg.GetResourceDef(itemId);

        if (resource == nullptr || !resource->isConsumable || resource->nutrition <= 0.0f) {
            continue;
        }

        while (count > 0 && remainingNutrition > 0.0f) {
            count--;
            remainingNutrition -= resource->nutrition;
        }

        if (count <= 0) {
            emptyItems.push_back(itemId);
        }
    }

    for (const std::string& itemId : emptyItems) {
        inventory.items.erase(itemId);
    }

    return true;
}

bool IsAdultHumanVillageMember(EntityID entity, const EntityManager& em, EntityID villageId) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasTag[entity] || !em.hasVillageMember[entity]) {
        return false;
    }

    const auto& tag = em.tags[entity];
    const auto& member = em.villageMembers[entity];

    return member.villageId == villageId && tag.species == "human" && tag.age >= 16;
}

EntityID FindAdultByGender(const EntityManager& em, EntityID villageId, const std::string& gender) {
    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!IsAdultHumanVillageMember(entity, em, villageId)) {
            continue;
        }

        if (em.tags[entity].gender == gender) {
            return entity;
        }
    }

    return static_cast<EntityID>(-1);
}

// ---------------------------------------------------------
// Added Social Setup Helpers for Reproduction V2
// ---------------------------------------------------------
bool IsValidPartnerLink(const EntityManager& em, EntityID a, EntityID b) {
    if (a >= em.active.size() || b >= em.active.size() || !em.active[a] || !em.active[b] || !em.hasFamily[a] || !em.hasFamily[b]) {
        return false;
    }

    return em.families[a].partnerId == b && em.families[b].partnerId == a;
}

bool IsAdultHumanCoupleMember(const EntityManager& em, EntityID entity, EntityID villageId) {
    if (!IsAdultHumanVillageMember(entity, em, villageId)) {
        return false;
    }

    if (!em.hasFamily[entity]) {
        return false;
    }

    return em.families[entity].partnerId != static_cast<EntityID>(-1);
}

bool AreBirthCompatiblePartners(const EntityManager& em, EntityID a, EntityID b, EntityID villageId) {
    if (!IsAdultHumanVillageMember(a, em, villageId) || !IsAdultHumanVillageMember(b, em, villageId)) {
        return false;
    }

    if (!IsValidPartnerLink(em, a, b)) {
        return false;
    }

    if (!em.hasTag[a] || !em.hasTag[b]) {
        return false;
    }

    const std::string& genderA = em.tags[a].gender;
    const std::string& genderB = em.tags[b].gender;

    if (genderA == "undefined" || genderB == "undefined") {
        return false;
    }

    // V2 keeps current binary reproduction simple.
    // Later this can become data-driven.
    return genderA != genderB;
}

bool FindEligibleCoupleForBirth(const EntityManager& em, EntityID villageId, EntityID& outParentA, EntityID& outParentB) {
    outParentA = static_cast<EntityID>(-1);
    outParentB = static_cast<EntityID>(-1);

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!IsAdultHumanCoupleMember(em, entity, villageId)) {
            continue;
        }

        const EntityID partner = em.families[entity].partnerId;

        if (partner == static_cast<EntityID>(-1)) {
            continue;
        }

        // Avoid evaluating the same couple twice.
        if (partner < entity) {
            continue;
        }

        if (!AreBirthCompatiblePartners(em, entity, partner, villageId)) {
            continue;
        }

        if (!HasFamilyHousingCapacityForNewChild(em, entity, partner)) {
            continue;
        }

        outParentA = entity;
        outParentB = partner;
        return true;
    }

    return false;
}

EntityID GetFamilyKey(EntityID a, EntityID b) {
    return std::min(a, b);
}

int CountActiveChildrenOfCouple(const EntityManager& em, EntityID parentA, EntityID parentB) {
    if (parentA >= em.active.size() || !em.active[parentA] || !em.hasFamily[parentA]) {
        return 0;
    }

    int count = 0;

    for (EntityID child : em.families[parentA].children) {
        if (child >= em.active.size() || !em.active[child] || !em.hasFamily[child]) {
            continue;
        }

        const FamilyComponent& childFamily = em.families[child];

        const bool sameParents = (childFamily.parentA == parentA && childFamily.parentB == parentB) ||
                                 (childFamily.parentA == parentB && childFamily.parentB == parentA);

        if (sameParents) {
            count++;
        }
    }

    return count;
}

int CountFamilySize(const EntityManager& em, EntityID parentA, EntityID parentB) {
    return 2 + CountActiveChildrenOfCouple(em, parentA, parentB);
}

int CountPrivateBedCapacityForFamily(const EntityManager& em, EntityID familyKey) {
    int capacity = 0;

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (entity >= em.active.size() || !em.active[entity] || !em.hasRestSpot[entity]) {
            continue;
        }

        if (em.hasBlueprint[entity] && !em.blueprints[entity].isFinished) {
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

bool HasFamilyHousingCapacityForNewChild(const EntityManager& em, EntityID parentA, EntityID parentB) {
    const EntityID familyKey = GetFamilyKey(parentA, parentB);
    const int currentFamilySize = CountFamilySize(em, parentA, parentB);
    const int familyBedCapacity = CountPrivateBedCapacityForFamily(em, familyKey);

    return currentFamilySize + 1 <= familyBedCapacity;
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

void RecomputeVillagePopulation(EntityManager& em, EntityID villageId) {
    if (villageId >= em.active.size() || !em.active[villageId] || !em.hasVillage[villageId]) {
        return;
    }

    int population = 0;
    int adults = 0;
    int children = 0;

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!em.active[entity] || !em.hasVillageMember[entity] || !em.hasTag[entity]) {
            continue;
        }

        if (em.villageMembers[entity].villageId != villageId) {
            continue;
        }

        population++;

        if (em.tags[entity].age >= 16) {
            adults++;
        } else {
            children++;
        }
    }

    em.villages[villageId].currentPopulation = population;
    em.villages[villageId].adultPopulation = adults;
    em.villages[villageId].childPopulation = children;
}

void AgeVillageMembersOneSeason(EntityManager& em, EntityID villageId) {
    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!em.active[entity] || !em.hasVillageMember[entity] || !em.hasTag[entity]) {
            continue;
        }

        if (em.villageMembers[entity].villageId != villageId) {
            continue;
        }

        em.tags[entity].age += 1;
    }
}

bool IsRestSpotUsableByVillage(const EntityManager& em, EntityID restSpotEntity, EntityID villageId) {
    if (restSpotEntity >= em.active.size() || !em.active[restSpotEntity] || !em.hasRestSpot[restSpotEntity]) {
        return false;
    }

    if (em.hasBlueprint[restSpotEntity] && !em.blueprints[restSpotEntity].isFinished) {
        return false;
    }

    const RestSpotComponent& restSpot = em.restSpots[restSpotEntity];

    if (restSpot.isPrivate) {
        return restSpot.ownerVillageId == villageId;
    }

    if (restSpot.ownerVillageId == static_cast<EntityID>(-1)) {
        return true;
    }

    return restSpot.ownerVillageId == villageId;
}

int CountVillageHousingCapacity(const EntityManager& em, EntityID villageId) {
    int capacity = 0;

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!IsRestSpotUsableByVillage(em, entity, villageId)) {
            continue;
        }

        capacity += std::max(0, em.restSpots[entity].capacity);
    }

    return capacity;
}

int GetEffectivePopulationLimit(const EntityManager& em, EntityID villageId) {
    if (villageId >= em.active.size() || !em.active[villageId] || !em.hasVillage[villageId]) {
        return 0;
    }

    const VillageComponent& village = em.villages[villageId];
    const int housingCapacity = CountVillageHousingCapacity(em, villageId);

    return std::min(village.populationLimit, housingCapacity);
}

bool HasHousingCapacityForNewChild(const EntityManager& em, EntityID villageId) {
    if (villageId >= em.active.size() || !em.active[villageId] || !em.hasVillage[villageId]) {
        return false;
    }

    const VillageComponent& village = em.villages[villageId];
    const int effectiveLimit = GetEffectivePopulationLimit(em, villageId);

    return village.currentPopulation + 1 <= effectiveLimit;
}

bool TrySpawnHumanChild(EntityManager& em, EntityRegistry& entityReg, const NameRegistry& nameReg, const BehaviorRegistry& behaviorReg,
                        const WorldMap& map, const TileRegistry& tileReg, EntityID villageId, EntityID parentA, EntityID parentB) {
    if (villageId >= em.active.size() || !em.active[villageId] || !em.hasTransform[villageId]) {
        return false;
    }

    const Vector2 spawnPos = FindVillagerSpawnPosition(em, map, tileReg, em.transforms[villageId].position, GetRandomValue(0, 11));

    EntityID child = entityReg.SpawnEntity(em, "VILLAGER", spawnPos, nameReg, behaviorReg);

    if (child >= em.active.size() || !em.active[child]) {
        return false;
    }

    if (em.hasTag[child]) {
        em.tags[child].age = 0;
    }

    if (em.hasProfession[child]) {
        em.professions[child].currentProfession = "none";
    }

    em.hasVillageMember[child] = true;
    em.villageMembers[child] = {villageId};

    EnsureFamilyComponent(em, child);
    EnsureFamilyComponent(em, parentA);
    EnsureFamilyComponent(em, parentB);

    em.families[child].parentA = parentA;
    em.families[child].parentB = parentB;

    if (parentA < em.active.size() && em.active[parentA]) {
        em.families[parentA].children.push_back(child);
        em.families[parentA].partnerId = parentB;
    }

    if (parentB < em.active.size() && em.active[parentB]) {
        em.families[parentB].children.push_back(child);
        em.families[parentB].partnerId = parentA;
    }

    return true;
}

} // namespace

EntityID VillageSystem::InitializeStartingVillage(EntityManager& em, EntityRegistry& entityReg, FurnitureRegistry& furnitureReg,
                                                  const NameRegistry& nameReg, const BehaviorRegistry& behaviorReg, const WorldMap& map,
                                                  const TileRegistry& tileReg, Vector2 preferredCenter) {
    const Vector2 villageCorePosition = FindValidWorldPositionNear(em, map, tileReg, preferredCenter, 30);

    EntityID villageCore = furnitureReg.SpawnFurniture(em, "VILLAGE_CORE", villageCorePosition, false);

    if (villageCore >= em.active.size() || !em.active[villageCore]) {
        return static_cast<EntityID>(-1);
    }

    em.hasVillage[villageCore] = true;
    em.villages[villageCore] = {"First Village", villageCorePosition, DEFAULT_VILLAGE_POPULATION_LIMIT, 0, 0, 0};

    AddStartingResources(em, villageCore);

    const int villagerCount = GetRandomValue(3, 4);

    for (int i = 0; i < villagerCount; ++i) {
        const Vector2 villagerPos = FindVillagerSpawnPosition(em, map, tileReg, villageCorePosition, i);

        EntityID villager = entityReg.SpawnEntity(em, "VILLAGER", villagerPos, nameReg, behaviorReg);

        if (villager >= em.active.size() || !em.active[villager]) {
            continue;
        }

        if (em.hasTag[villager]) {
            em.tags[villager].age = GetRandomValue(16, 35);
        }

        if (em.hasProfession[villager]) {
            em.professions[villager].currentProfession = "none";
        }

        em.hasVillageMember[villager] = true;
        em.villageMembers[villager] = {villageCore};
    }

    RecomputeVillagePopulation(em, villageCore);

    return villageCore;
}

void VillageSystem::Update(float, EntityManager& em, EntityRegistry& entityReg, const NameRegistry& nameReg,
                           const BehaviorRegistry& behaviorReg, const WorldMap& map, const TileRegistry& tileReg,
                           const ResourceRegistry& resourceReg, const TimeSystem& timeSystem) {
    const int currentSeasonNumber = timeSystem.GetSeasonNumber();

    if (m_lastProcessedSeasonNumber == 0) {
        m_lastProcessedSeasonNumber = currentSeasonNumber;

        for (EntityID villageId = 0; villageId < em.active.size(); ++villageId) {
            if (em.active[villageId] && em.hasVillage[villageId]) {
                RecomputeVillagePopulation(em, villageId);
            }
        }
        return;
    }

    for (EntityID villageId = 0; villageId < em.active.size(); ++villageId) {
        if (em.active[villageId] && em.hasVillage[villageId]) {
            RecomputeVillagePopulation(em, villageId);
        }
    }

    if (currentSeasonNumber == m_lastProcessedSeasonNumber) {
        return;
    }

    m_lastProcessedSeasonNumber = currentSeasonNumber;

    for (EntityID villageId = 0; villageId < em.active.size(); ++villageId) {
        if (!em.active[villageId] || !em.hasVillage[villageId]) {
            continue;
        }

        AgeVillageMembersOneSeason(em, villageId);
        RecomputeVillagePopulation(em, villageId);

        if (!em.hasInventory[villageId]) {
            continue;
        }

        if (!HasHousingCapacityForNewChild(em, villageId)) {
            continue;
        }

        // Updated Partner Check
        EntityID parentA = static_cast<EntityID>(-1);
        EntityID parentB = static_cast<EntityID>(-1);

        if (!FindEligibleCoupleForBirth(em, villageId, parentA, parentB)) {
            continue;
        }

        if (!ConsumeFoodNutritionFromInventory(em.inventories[villageId], resourceReg, BIRTH_FOOD_NUTRITION_COST)) {
            continue;
        }

        if (TrySpawnHumanChild(em, entityReg, nameReg, behaviorReg, map, tileReg, villageId, parentA, parentB)) {
            RecomputeVillagePopulation(em, villageId);
        }
    }
}
