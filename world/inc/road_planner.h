/**
 * @file road_planner.h
 * @brief Helpers to derive road routes between structure waypoints.
 */

#ifndef ROAD_PLANNER_H
#define ROAD_PLANNER_H

typedef struct RoadPoint
{
    int x;
    int y;
} RoadPoint;

/**
 * @brief Computes an order that approximates the shortest path visiting all points once.
 *
 * The first point is treated as fixed anchor and the returned sequence always starts there.
 * @param points       Array of waypoints to connect.
 * @param count        Number of waypoints in the array.
 * @param outOrder     Output buffer receiving the index order (length = count).
 * @param orderCapacity Capacity of @p outOrder (must be >= count).
 * @return Number of indices written to @p outOrder (0 on failure).
 */
int tsp_plan_route(const RoadPoint* points, int count, int* outOrder, int orderCapacity);

#endif /* ROAD_PLANNER_H */
