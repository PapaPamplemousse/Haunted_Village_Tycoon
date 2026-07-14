/**
 * @file AIIntent.hpp
 * @brief Describes an AI action intent before it is applied to a BehaviorComponent.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "ecs/Components.hpp"

#include <raylib.h>
#include <string>

enum class AIIntentKind { None, StartAction, MoveAdjacentToEntity };

struct AIIntent {
    AIIntentKind kind = AIIntentKind::None;

    std::string moveTask;
    std::string actionTask;

    EntityID targetEntity = static_cast<EntityID>(-1);
    Vector2 targetPosition = {0.0f, 0.0f};

    float actionDuration = 0.0f;
};
