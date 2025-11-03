#ifndef PATHFINDING_H
#define PATHFINDING_H

#include <stdbool.h>

#include "raylib.h"
#include "world.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PATHFINDING_MAX_LENGTH 256

typedef struct PathfindingPath
{
    Vector2 points[PATHFINDING_MAX_LENGTH];
    int     count;
} PathfindingPath;

typedef struct PathfindingOptions
{
    bool  allowDiagonal;
    bool  canOpenDoors;
    float agentRadius;
} PathfindingOptions;

bool pathfinding_find_path(const Map* map,
                           Vector2 start,
                           Vector2 goal,
                           const PathfindingOptions* options,
                           PathfindingPath* outPath);

#ifdef __cplusplus
}
#endif

#endif /* PATHFINDING_H */
