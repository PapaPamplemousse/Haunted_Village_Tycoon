#include "systems/BuildPlacementSystem.hpp"

#include "core/Config.hpp"

void BuildPlacementSystem::Update(const InputManager& inputManager, const UIManager& uiManager, EntityManager& entityManager,
                                  EntityRegistry& entityRegistry, FurnitureRegistry& furnitureRegistry,
                                  ConstructionRegistry& constructionRegistry, const NameRegistry& nameRegistry,
                                  const BehaviorRegistry& behaviorRegistry) const {
    const std::string prefabToPlace = uiManager.GetSelectedPrefab();

    if (inputManager.IsInteractPressed() && !prefabToPlace.empty()) {
        Vector2 spawnPos = {inputManager.GetMouseGridX() * Config::TILE_SIZE + (Config::TILE_SIZE / 2.0f),
                            inputManager.GetMouseGridY() * Config::TILE_SIZE + (Config::TILE_SIZE / 2.0f)};

        if (uiManager.GetSelectedCategory() == BuildCategory::Entities) {
            entityRegistry.SpawnEntity(entityManager, prefabToPlace, spawnPos, nameRegistry, behaviorRegistry);
        } else if (uiManager.GetSelectedCategory() == BuildCategory::Furniture) {
            furnitureRegistry.SpawnFurniture(entityManager, prefabToPlace, spawnPos, true);
        } else if (uiManager.GetSelectedCategory() == BuildCategory::Constructions) {
            constructionRegistry.SpawnConstruction(entityManager, prefabToPlace, spawnPos, true);
        }
    }

    if (!inputManager.IsDeletePressed()) {
        return;
    }

    const int targetX = inputManager.GetMouseGridX();
    const int targetY = inputManager.GetMouseGridY();

    for (size_t i = 0; i < entityManager.active.size(); ++i) {
        if (!entityManager.active[i] || !entityManager.hasTransform[i]) {
            continue;
        }

        const int entityGridX = static_cast<int>(entityManager.transforms[i].position.x / Config::TILE_SIZE);

        const int entityGridY = static_cast<int>(entityManager.transforms[i].position.y / Config::TILE_SIZE);

        if (entityGridX != targetX || entityGridY != targetY) {
            continue;
        }

        if (entityManager.hasBlueprint[i] && !entityManager.blueprints[i].isFinished) {
            entityManager.DestroyEntity(i);
        } else if (!entityManager.hasDeconstruct[i] && !entityManager.hasBehavior[i]) {
            entityManager.hasDeconstruct[i] = true;
            entityManager.deconstructs[i] = {true};
        }

        break;
    }
}
