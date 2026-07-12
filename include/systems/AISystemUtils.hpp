#pragma once

#include "data/ResourceRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "world/EntitySpatialGrid.hpp"

#include <raylib.h>
#include <string>

namespace AISystemUtils {

constexpr float ATTACK_DURATION = 0.45f;
constexpr float ATTACK_COOLDOWN = 0.6f;

constexpr float SEEK_FOOD_THRESHOLD_RATIO = 0.5f;
constexpr float EAT_DURATION = 1.0f;
constexpr float DEPOSIT_DURATION = 0.8f;

int ToTileCoord(float worldCoord);

float SquaredDistance(Vector2 a, Vector2 b);

bool IsWithinActiveSimulationRadius(Vector2 entityPosition, Vector2 simulationCenter, float activeRadiusTiles);

float GetActionRadiusWorld(EntityID entity, const EntityManager& em);

void GiveLootToInventory(EntityID receiver, EntityID source, EntityManager& em);

bool IsConsumableFoodItem(const ResourceRegistry& resourceReg, const std::string& itemId);

std::string FindFirstFoodItemInInventory(const InventoryComponent& inventory, const ResourceRegistry& resourceReg);

bool ConsumeFoodFromInventory(InventoryComponent& inventory, NeedsComponent& needs, const std::string& itemId,
                              const ResourceRegistry& resourceReg);

bool HarvestableHasFoodDrop(const HarvestableComponent& harvestable, const ResourceRegistry& resourceReg);

bool HasRequiredHarvestTool(EntityID worker, const HarvestableComponent& harvestable, const EntityManager& em);

int GetInventoryItemCount(const InventoryComponent& inventory);

bool HasAnyInventoryItem(const InventoryComponent& inventory);

bool StorageAcceptsItem(const StorageComponent& storage, const std::string& itemId);

bool HasAvailableStorageCapacity(EntityID storageEntity, const EntityManager& em);

bool StorageCanAcceptFromInventory(EntityID storageEntity, const InventoryComponent& sourceInventory, const EntityManager& em);

void DepositInventoryIntoStorage(InventoryComponent& sourceInventory, InventoryComponent& storageInventory,
                                 const StorageComponent& storage);

bool IsHourInRange(float hour, float startHour, float endHour);

bool CanStartWorkNow(const BehaviorComponent& behavior, float currentHour);

bool ShouldDepositInventory(EntityID entity, const EntityManager& em, const BehaviorComponent& behavior, float currentHour);

int GetItemCount(const InventoryComponent& inventory, const std::string& itemId);

int RemoveItemFromInventory(InventoryComponent& inventory, const std::string& itemId, int amount);

std::vector<EntityID> GetAccessibleStorageEntities(EntityID entity, const EntityManager& em, const EntitySpatialGrid& spatialGrid);

int CountAccessibleItem(EntityID entity, const EntityManager& em, const EntitySpatialGrid& spatialGrid, const std::string& itemId);

bool HasAccessibleMaterials(EntityID entity, const EntityManager& em, const EntitySpatialGrid& spatialGrid,
                            const std::unordered_map<std::string, int>& requiredMaterials);

bool ConsumeAccessibleMaterials(EntityID entity, EntityManager& em, const EntitySpatialGrid& spatialGrid,
                                const std::unordered_map<std::string, int>& requiredMaterials);

std::vector<EntityID> GetAccessibleStorageEntities(EntityID entity, const EntityManager& em, const EntitySpatialGrid& spatialGrid);

bool ShouldRest(EntityID entity, const EntityManager& em, const BehaviorComponent& behavior, float currentHour);

bool IsFullyRested(EntityID entity, const EntityManager& em);

void CleanRestSpotOccupants(RestSpotComponent& restSpot, const EntityManager& em);

bool RestSpotHasCapacity(EntityID restSpotEntity, EntityManager& em);

bool ReserveRestSpot(EntityID restSpotEntity, EntityID sleeper, EntityManager& em);

void ReleaseRestSpotReservation(EntityID sleeper, EntityManager& em);

} // namespace AISystemUtils
