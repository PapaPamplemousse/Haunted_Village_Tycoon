#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "building.h"
#include "tile.h"
#include "object.h"

/* ===========================================
 * Structures
 * =========================================== */
typedef struct
{
    int       area;
    Rectangle bounds;
    int       doorCount;
    int       wallBoundaryCount;
    bool      nonStructuralBlocker;
    bool      touchesBorder;
} FloodResult;

Building buildings[MAX_BUILDINGS];
int      buildingCount = 0;

/* ===========================================
 * 1. Object analysis utility functions
 * =========================================== */

/**
 * @brief Checks if an object is part of a building's structure.
 * (Wall, door, or any future structural element.)
 */
static inline bool is_structural_object(const Object* obj)
{
    if (!obj)
        return false;

    return is_wall_object(obj) || is_door_object(obj);
}

/**
 * @brief Checks if an object blocks walking without being structural.
 * Example: bed, table, chest -> blocks, but doesn't form a wall.
 */
static inline bool is_non_structural_blocker(const Object* obj)
{
    if (!obj)
        return false;

    if (is_structural_object(obj))
        return false;

    return !obj->type->walkable;
}

/**
 * @brief Determines if an object contributes to the "structural boundary".
 * I.e., if it encloses the room.
 */
static inline bool contributes_to_building_boundary(const Object* obj)
{
    if (!obj)
        return false;

    // Wall or door encloses the room
    if (is_wall_object(obj) || is_door_object(obj))
        return true;

    /* FOR FUTURE ???*/
    // if (is_magic_barrier(obj)) return true;
    // if (is_force_field(obj)) return true;

    return false;
}

/* ===========================================
 * 2. Main Flood-fill
 * =========================================== */

/**
 * @brief Flood-fills an empty area to identify a potential room.
 *
 * Rules:
 * - Walls and doors are considered structural (boundary, blocking).
 * - Non-structural furniture (bed, table, etc.) invalidates the room.
 * - Walkable objects do not block.
 * - A room is valid if it is enclosed and does not touch the border.
 */
static FloodResult perform_flood_fill(Map* map, int sx, int sy, bool visited[MAP_HEIGHT][MAP_WIDTH])
{
    FloodResult res = {0};

    int minx = map->width, miny = map->height;
    int maxx = -1, maxy = -1;

    const int stackCap = map->width * map->height;
    int*      stack    = (int*)malloc(stackCap * sizeof(int));
    int       top      = 0;

    visited[sy][sx] = true;
    stack[top++]    = sy * map->width + sx;

    while (top > 0)
    {
        const int idx = stack[--top];
        const int cx  = idx % map->width;
        const int cy  = idx / map->width;

        res.area++;
        if (cx < minx)
            minx = cx;
        if (cy < miny)
            miny = cy;
        if (cx > maxx)
            maxx = cx;
        if (cy > maxy)
            maxy = cy;

        static const int DIRS[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
        for (int d = 0; d < 4; ++d)
        {
            const int nx = cx + DIRS[d][0];
            const int ny = cy + DIRS[d][1];

            if (nx < 0 || ny < 0 || nx >= map->width || ny >= map->height)
            {
                res.touchesBorder = true;
                continue;
            }

            if (visited[ny][nx])
                continue;

            Object* obj = map->objects[ny][nx];

            if (!obj)
            {
                visited[ny][nx] = true;
                stack[top++]    = ny * map->width + nx;
            }
            else if (contributes_to_building_boundary(obj))
            {
                // Wall or door = boundary
                if (is_wall_object(obj))
                    res.wallBoundaryCount++;
                else if (is_door_object(obj))
                    res.doorCount++;

                // We don't cross a wall/door: the room stops here
                continue;
            }
            else if (is_non_structural_blocker(obj))
            {
                // Checks if the furniture touches an unvisited outside area (open wall, map border)
                bool touchesOutside = false;
                for (int dd = 0; dd < 4; ++dd)
                {
                    int tx = nx + DIRS[dd][0];
                    int ty = ny + DIRS[dd][1];
                    if (tx < 0 || ty < 0 || tx >= map->width || ty >= map->height)
                    {
                        touchesOutside = true;
                        break;
                    }
                    // Only consider “outside” if you go outside the limits of the map
                    if (tx < 0 || ty < 0 || tx >= map->width || ty >= map->height)
                    {
                        touchesOutside = true;
                        break;
                    }
                }

                if (touchesOutside)
                {
                    // Furniture adjacent to the border or outside: invalidates the room
                    res.nonStructuralBlocker = true;
                }

                // In any case, mark the tile to prevent an infinite loop
                visited[ny][nx] = true;
                stack[top++]    = ny * map->width + nx;
            }
            else
            {
                // Walkable or decorative object
                visited[ny][nx] = true;
                stack[top++]    = ny * map->width + nx;
            }
        }
    }

    free(stack);

    res.bounds.x      = (float)minx;
    res.bounds.y      = (float)miny;
    res.bounds.width  = (float)(maxx - minx + 1);
    res.bounds.height = (float)(maxy - miny + 1);

    return res;
}

/* ===========================================
 * 3. Room validation
 * =========================================== */
static bool is_valid_building_area(const FloodResult* r)
{
    if (r->area <= 0)
        return false;

    if (r->touchesBorder)
        return false;

    if (r->nonStructuralBlocker)
        return false;

    // At least one structural element (wall or door)
    if ((r->wallBoundaryCount + r->doorCount) <= 0)
        return false;

    return true;
}

/* ===========================================
 * 4. Initialization and collection
 * =========================================== */
static void init_building_structure(Building* b, int id, const FloodResult* res)
{
    b->id          = id;
    b->bounds      = res->bounds;
    b->area        = res->area;
    b->center      = (Vector2){res->bounds.x + res->bounds.width / 2.0f, res->bounds.y + res->bounds.height / 2.0f};
    b->objectCount = 0;
    b->objects     = NULL;
    b->roomType    = NULL;
}

static void collect_building_objects(Map* map, Building* b, const FloodResult* res, const bool visited[MAP_HEIGHT][MAP_WIDTH])
{
    Object** temp_objects    = (Object**)malloc(res->area * sizeof(Object*));
    int      collected_count = 0;

    for (int y = (int)res->bounds.y; y < res->bounds.y + res->bounds.height; ++y)
    {
        for (int x = (int)res->bounds.x; x < res->bounds.x + res->bounds.width; ++x)
        {
            if (!visited[y][x])
                continue;

            Object* obj = map->objects[y][x];
            if (!obj)
                continue;

            // We do not collect walls and doors (structural boundaries)
            if (is_wall_object(obj) || is_door_object(obj))
                continue;

            // All other interior objects (bed, table, torch, decor...) are collected
            temp_objects[collected_count++] = obj;
        }
    }

    b->objectCount = collected_count;
    if (collected_count > 0)
    {
        b->objects = (Object**)malloc(collected_count * sizeof(Object*));
        memcpy(b->objects, temp_objects, collected_count * sizeof(Object*));
    }
    else
    {
        b->objects = NULL;
    }

    free(temp_objects);
}

/* ===========================================
 * 5. Main detection
 * =========================================== */
void update_building_detection(Map* map)
{
    buildingCount = 0;

    static bool visited[MAP_HEIGHT][MAP_WIDTH];
    memset(visited, 0, sizeof(visited));

    int nextId = 1;

    for (int y = 0; y < map->height; ++y)
    {
        for (int x = 0; x < map->width; ++x)
        {
            if (visited[y][x])
                continue;

            Object* obj = map->objects[y][x];

            // Ignore walls, doors, and blocking obstacles
            if (obj && (is_structural_object(obj) || is_non_structural_blocker(obj)))
            {
                visited[y][x] = true;
                continue;
            }

            FloodResult res = perform_flood_fill(map, x, y, visited);
            if (!is_valid_building_area(&res))
                continue;

            if (buildingCount >= MAX_BUILDINGS)
                continue;

            Building* b = &buildings[buildingCount];
            init_building_structure(b, nextId, &res);
            collect_building_objects(map, b, &res, visited);

            // Classification
            const RoomTypeRule* rule = analyze_building_type(b);
            if (rule)
            {
                snprintf(b->name, sizeof(b->name), "%s", rule->name);
                b->roomType = rule;
            }
            else
            {
                snprintf(b->name, sizeof(b->name), "Unclassified Room");
                b->roomType = NULL;
            }

            buildingCount++;
            nextId++;
        }
    }
}
