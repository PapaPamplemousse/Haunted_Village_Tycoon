#include "road_planner.h"

#include <stdbool.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>

static inline int manhattan_distance(const RoadPoint* a, const RoadPoint* b)
{
    return abs(a->x - b->x) + abs(a->y - b->y);
}

static int route_cost(const RoadPoint* points, const int* order, int count)
{
    if (!points || !order || count <= 1)
        return 0;

    int cost = 0;
    for (int i = 0; i < count - 1; ++i)
        cost += manhattan_distance(&points[order[i]], &points[order[i + 1]]);
    return cost;
}

typedef struct
{
    const RoadPoint* points;
    int              count;
    int              bestCost;
    int*             bestOrder;
} TSPState;

static void tsp_permute(TSPState* state, int* currentOrder, int startIndex)
{
    if (startIndex == state->count)
    {
        int cost = route_cost(state->points, currentOrder, state->count);
        if (cost < state->bestCost)
        {
            state->bestCost = cost;
            memcpy(state->bestOrder, currentOrder, sizeof(int) * state->count);
        }
        return;
    }

    for (int i = startIndex; i < state->count; ++i)
    {
        int tmp                   = currentOrder[startIndex];
        currentOrder[startIndex]  = currentOrder[i];
        currentOrder[i]           = tmp;

        tsp_permute(state, currentOrder, startIndex + 1);

        currentOrder[i]           = currentOrder[startIndex];
        currentOrder[startIndex]  = tmp;
    }
}

static void tsp_greedy(const RoadPoint* points, int count, int* outOrder)
{
    bool visited[32] = {false};
    if (count > (int)(sizeof(visited) / sizeof(visited[0])))
        memset(visited, 0, sizeof(visited));

    outOrder[0] = 0;
    if (count <= 1)
        return;

    visited[0] = true;
    for (int i = 1; i < count; ++i)
    {
        int  lastIndex = outOrder[i - 1];
        int  bestIdx   = -1;
        int  bestCost  = INT_MAX;
        for (int candidate = 1; candidate < count; ++candidate)
        {
            if (visited[candidate])
                continue;
            int cost = manhattan_distance(&points[lastIndex], &points[candidate]);
            if (cost < bestCost)
            {
                bestCost = cost;
                bestIdx  = candidate;
            }
        }
        if (bestIdx < 0)
            bestIdx = i;
        visited[bestIdx] = true;
        outOrder[i]      = bestIdx;
    }
}

static void two_opt_pass(const RoadPoint* points, int count, int* order)
{
    if (count <= 3)
        return;

    bool improved = true;
    while (improved)
    {
        improved = false;
        for (int i = 1; i < count - 2; ++i)
        {
            for (int k = i + 1; k < count - 1; ++k)
            {
                RoadPoint a1 = points[order[i - 1]];
                RoadPoint a2 = points[order[i]];
                RoadPoint b1 = points[order[k]];
                RoadPoint b2 = points[order[k + 1]];

                int current = manhattan_distance(&a1, &a2) + manhattan_distance(&b1, &b2);
                int swapped = manhattan_distance(&a1, &b1) + manhattan_distance(&a2, &b2);

                if (swapped < current)
                {
                    for (int left = i, right = k; left < right; ++left, --right)
                    {
                        int tmp     = order[left];
                        order[left] = order[right];
                        order[right] = tmp;
                    }
                    improved = true;
                }
            }
        }
    }
}

int tsp_plan_route(const RoadPoint* points, int count, int* outOrder, int orderCapacity)
{
    if (!points || !outOrder || count <= 0 || orderCapacity < count)
        return 0;

    if (count == 1)
    {
        outOrder[0] = 0;
        return 1;
    }

    int tempOrder[32];
    int bestOrder[32];
    if (count > (int)(sizeof(tempOrder) / sizeof(tempOrder[0])))
    {
        // Fallback to greedy plan when we exceed static buffers.
        tsp_greedy(points, count, outOrder);
        return count;
    }

    for (int i = 0; i < count; ++i)
        tempOrder[i] = i;

    int  bestCost = INT_MAX;
    bool usedBrute = false;
    if (count <= 9)
    {
        TSPState state = {
            .points   = points,
            .count    = count,
            .bestCost = bestCost,
            .bestOrder = bestOrder,
        };
        state.bestCost = INT_MAX;
        tsp_permute(&state, tempOrder, 1);
        if (state.bestCost < INT_MAX)
        {
            memcpy(outOrder, state.bestOrder, sizeof(int) * count);
            usedBrute = true;
        }
    }

    if (!usedBrute)
    {
        tsp_greedy(points, count, outOrder);
        two_opt_pass(points, count, outOrder);
    }

    return count;
}
