/**
 * @file EventSystemSpawn.cpp
 * @brief EventSystem helpers for spawning event-driven entity waves.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/EventSystem.hpp"

#include <algorithm>
#include <cmath>
#include <raylib.h>

namespace {

int WorldToTile(float worldCoord) {
    return static_cast<int>(std::floor(worldCoord / Config::TILE_SIZE));
}

Vector2 TileToWorldCenter(int tileX, int tileY) {
    return {tileX * Config::TILE_SIZE + Config::TILE_SIZE * 0.5f, tileY * Config::TILE_SIZE + Config::TILE_SIZE * 0.5f};
}

bool IsEntityOnTile(const EntityManager& em, int tileX, int tileY) {
    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!em.active[entity] || !em.hasTransform[entity]) {
            continue;
        }

        const int ex = WorldToTile(em.transforms[entity].position.x);
        const int ey = WorldToTile(em.transforms[entity].position.y);

        if (ex == tileX && ey == tileY) {
            return true;
        }
    }

    return false;
}

bool IsBlockedTileById(const TileRegistry& tileReg, int tileId) {
    const int voidTile = tileReg.GetTileIdByString("VOID");
    const int lavaTile = tileReg.GetTileIdByString("LAVA");

    if (voidTile >= 0 && tileId == voidTile) {
        return true;
    }

    if (lavaTile >= 0 && tileId == lavaTile) {
        return true;
    }

    return tileId < 0;
}

} // namespace

bool EventSystem::TriggerSpawnWaveRule(EntityManager& em, EntityRegistry& entityReg, const NameRegistry& nameReg,
                                       const BehaviorRegistry& behaviorReg, const EventRuleDef& rule, const WorldMap& worldMap,
                                       const TileRegistry& tileReg, const TimeSystem& timeSystem, SettlementMetrics& metrics,
                                       Chronicle& chronicle) {
    if (rule.spawnPrefabId.empty()) {
        return false;
    }

    const EntityID villageId = FindPrimaryVillage(em);

    if (villageId == static_cast<EntityID>(-1)) {
        return false;
    }

    const int population = CountVillagePopulation(em, villageId);

    int spawnCount = GetRandomValue(rule.spawnCount.min, rule.spawnCount.max);

    if (rule.spawnPopulationDivisor > 0) {
        spawnCount += population / rule.spawnPopulationDivisor;
    }

    if (rule.spawnSeasonDivisor > 0) {
        spawnCount += timeSystem.GetSeasonNumber() / rule.spawnSeasonDivisor;
    }

    if (rule.spawnRandomBonus > 0) {
        spawnCount += GetRandomValue(0, rule.spawnRandomBonus);
    }

    spawnCount = std::clamp(spawnCount, rule.spawnCount.min, rule.spawnCount.max);

    int spawned = 0;

    for (int i = 0; i < spawnCount; ++i) {
        Vector2 spawnPosition = {0.0f, 0.0f};

        const bool found = FindZombieSpawnPositionNearVillage(em, worldMap, tileReg, villageId, rule.spawnRadiusTiles.min,
                                                              rule.spawnRadiusTiles.max, spawnPosition);

        if (!found) {
            continue;
        }

        const EntityID spawnedEntity = entityReg.SpawnEntity(em, rule.spawnPrefabId, spawnPosition, nameReg, behaviorReg);

        if (spawnedEntity < em.active.size() && em.active[spawnedEntity]) {
            spawned++;
        }
    }

    if (spawned > 0) {
        if (rule.fear > 0.0f) {
            AddFear(metrics, rule.fear + static_cast<float>(spawned) * 1.5f);
        }

        if (rule.corruption > 0.0f) {
            AddCorruption(metrics, rule.corruption);
        }

        chronicle.Add(timeSystem.GetDay(), timeSystem.GetSeasonName(), PhaseToString(GetCurrentPhase(timeSystem.GetHour())),
                      FormatEventMessage(rule.message, spawned, "", 0));

        return true;
    }

    chronicle.Add(timeSystem.GetDay(), timeSystem.GetSeasonName(), PhaseToString(GetCurrentPhase(timeSystem.GetHour())),
                  rule.emptyMessage.empty() ? "Something stirred in the dark, but nothing reached the village." : rule.emptyMessage);

    return true;
}

EntityID EventSystem::FindPrimaryVillage(const EntityManager& em) const {
    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (em.active[entity] && em.hasVillage[entity] && em.hasTransform[entity]) {
            return entity;
        }
    }

    return static_cast<EntityID>(-1);
}

int EventSystem::CountVillagePopulation(const EntityManager& em, EntityID villageId) const {
    if (villageId >= em.active.size() || !em.active[villageId]) {
        return 0;
    }

    int count = 0;

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!em.active[entity] || !em.hasVillageMember[entity]) {
            continue;
        }

        if (em.villageMembers[entity].villageId == villageId) {
            count++;
        }
    }

    return count;
}

bool EventSystem::FindZombieSpawnPositionNearVillage(const EntityManager& em, const WorldMap& worldMap, const TileRegistry& tileReg,
                                                     EntityID villageId, int minRadiusTiles, int maxRadiusTiles,
                                                     Vector2& outPosition) const {
    if (villageId >= em.active.size() || !em.active[villageId] || !em.hasTransform[villageId]) {
        return false;
    }

    const int width = worldMap.GetWidth();
    const int height = worldMap.GetHeight();

    const int centerX = WorldToTile(em.transforms[villageId].position.x);
    const int centerY = WorldToTile(em.transforms[villageId].position.y);

    constexpr int MAX_ATTEMPTS = 80;

    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        const int radius = GetRandomValue(minRadiusTiles, maxRadiusTiles);
        const int angleDeg = GetRandomValue(0, 359);
        const float angleRad = static_cast<float>(angleDeg) * DEG2RAD;

        const int x = centerX + static_cast<int>(std::round(std::cos(angleRad) * static_cast<float>(radius)));
        const int y = centerY + static_cast<int>(std::round(std::sin(angleRad) * static_cast<float>(radius)));

        if (x < 2 || y < 2 || x >= width - 2 || y >= height - 2) {
            continue;
        }

        const int tileId = worldMap.GetTile(x, y);

        if (IsBlockedTileById(tileReg, tileId)) {
            continue;
        }

        if (IsEntityOnTile(em, x, y)) {
            continue;
        }

        outPosition = TileToWorldCenter(x, y);
        return true;
    }

    return false;
}
