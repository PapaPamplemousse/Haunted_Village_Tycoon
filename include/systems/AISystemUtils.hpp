#pragma once

#include "data/ResourceRegistry.hpp"
#include "ecs/EntityManager.hpp"

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

} // namespace AISystemUtils
