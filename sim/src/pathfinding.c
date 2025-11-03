// #include "pathfinding.h"

// #include <float.h>
// #include <math.h>
// #include <stdlib.h>
// #include <string.h>

// #include "object.h"
// #include "tile.h"

// #define PATHFINDING_MAX_NODES   4096
// #define PATHFINDING_MAX_EXTENT  30

// typedef struct Node
// {
//     int   x;
//     int   y;
//     float g;
//     float h;
//     int   parent;
//     bool  open;
//     bool  closed;
// } Node;

// static inline float heuristic_cost(int x0, int y0, int x1, int y1)
// {
//     return (float)(abs(x0 - x1) + abs(y0 - y1));
// }

// static inline float tile_cost(const TileType* tile)
// {
//     if (!tile)
//         return 1.0f;
//     float cost = tile->movementCost;
//     if (cost <= 0.01f)
//         cost = 1.0f;
//     return cost;
// }

// static bool tile_walkable(const Map* map, int x, int y, const PathfindingOptions* options)
// {
//     if (!map)
//         return false;
//     if (x < 0 || y < 0 || x >= map->width || y >= map->height)
//         return false;

//     TileTypeID tileId = map->tiles[y][x];
//     TileType*  tile   = get_tile_type(tileId);
//     if (!tile || !tile->walkable)
//         return false;

//     Object* obj = map->objects[y][x];
//     if (!obj)
//         return true;

//     if (object_is_walkable(obj))
//         return true;

//     if (options && options->canOpenDoors && obj->type && obj->type->isDoor)
//         return true;

//     return false;
// }

// static void reconstruct_path(const Node* nodes,
//                              int currentIndex,
//                              PathfindingPath* outPath)
// {
//     if (!outPath)
//         return;

//     Vector2 reverseBuffer[PATHFINDING_MAX_LENGTH];
//     int     length = 0;

//     while (currentIndex >= 0 && length < PATHFINDING_MAX_LENGTH)
//     {
//         const Node* n = &nodes[currentIndex];
//         reverseBuffer[length++] = (Vector2){(n->x + 0.5f) * TILE_SIZE, (n->y + 0.5f) * TILE_SIZE};
//         currentIndex            = n->parent;
//     }

//     outPath->count = 0;
//     for (int i = length - 1; i >= 0; --i)
//     {
//         outPath->points[outPath->count++] = reverseBuffer[i];
//     }
// }

// bool pathfinding_find_path(const Map* map,
//                            Vector2 start,
//                            Vector2 goal,
//                            const PathfindingOptions* options,
//                            PathfindingPath* outPath)
// {
//     if (outPath)
//         memset(outPath, 0, sizeof(*outPath));

//     if (!map)
//         return false;

//     int sx = (int)floorf(start.x / TILE_SIZE);
//     int sy = (int)floorf(start.y / TILE_SIZE);
//     int gx = (int)floorf(goal.x / TILE_SIZE);
//     int gy = (int)floorf(goal.y / TILE_SIZE);

//     if (sx == gx && sy == gy)
//     {
//         if (outPath)
//         {
//             outPath->points[0] = (Vector2){(gx + 0.5f) * TILE_SIZE, (gy + 0.5f) * TILE_SIZE};
//             outPath->count     = 1;
//         }
//         return true;
//     }

//     if (!tile_walkable(map, sx, sy, options) || !tile_walkable(map, gx, gy, options))
//         return false;

//     int halfExtent = PATHFINDING_MAX_EXTENT;
//     int minX = sx < gx ? sx : gx;
//     int minY = sy < gy ? sy : gy;
//     int maxX = sx > gx ? sx : gx;
//     int maxY = sy > gy ? sy : gy;

//     while (true)
//     {
//         int loX = minX - halfExtent;
//         int hiX = maxX + halfExtent;
//         int loY = minY - halfExtent;
//         int hiY = maxY + halfExtent;

//         if (loX < 0)
//             loX = 0;
//         if (loY < 0)
//             loY = 0;
//         if (hiX >= map->width)
//             hiX = map->width - 1;
//         if (hiY >= map->height)
//             hiY = map->height - 1;

//         int width  = hiX - loX + 1;
//         int height = hiY - loY + 1;
//         if (width * height <= PATHFINDING_MAX_NODES)
//         {
//             minX = loX;
//             maxX = hiX;
//             minY = loY;
//             maxY = hiY;
//             break;
//         }

//         halfExtent -= 4;
//         if (halfExtent <= 4)
//             return false;
//     }

//     int width  = maxX - minX + 1;
//     int height = maxY - minY + 1;
//     int total  = width * height;

//     Node nodes[PATHFINDING_MAX_NODES];
//     for (int i = 0; i < total; ++i)
//     {
//         nodes[i].x      = minX + (i % width);
//         nodes[i].y      = minY + (i / width);
//         nodes[i].g      = FLT_MAX;
//         nodes[i].h      = 0.0f;
//         nodes[i].parent = -1;
//         nodes[i].open   = false;
//         nodes[i].closed = false;
//     }

//     int startIndex = (sy - minY) * width + (sx - minX);
//     int goalIndex  = (gy - minY) * width + (gx - minX);

//     nodes[startIndex].g    = 0.0f;
//     nodes[startIndex].h    = heuristic_cost(sx, sy, gx, gy);
//     nodes[startIndex].open = true;

//     int openList[PATHFINDING_MAX_NODES];
//     int openCount = 0;
//     openList[openCount++] = startIndex;

//     static const int OFFSETS[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

//     while (openCount > 0)
//     {
//         int   bestIdx = 0;
//         float bestF   = FLT_MAX;
//         for (int i = 0; i < openCount; ++i)
//         {
//             int   nodeIndex = openList[i];
//             Node* node      = &nodes[nodeIndex];
//             float f         = node->g + node->h;
//             if (f < bestF)
//             {
//                 bestF   = f;
//                 bestIdx = i;
//             }
//         }

//         int currentIndex = openList[bestIdx];
//         Node* current    = &nodes[currentIndex];

//         openList[bestIdx] = openList[--openCount];
//         current->open     = false;
//         current->closed   = true;

//         if (currentIndex == goalIndex)
//         {
//             if (outPath)
//                 reconstruct_path(nodes, currentIndex, outPath);
//             return true;
//         }

//         for (int n = 0; n < 4; ++n)
//         {
//             int nx = current->x + OFFSETS[n][0];
//             int ny = current->y + OFFSETS[n][1];

//             if (!tile_walkable(map, nx, ny, options))
//                 continue;

//             int neighborIndex = (ny - minY) * width + (nx - minX);
//             if (neighborIndex < 0 || neighborIndex >= total)
//                 continue;

//             Node* neighbor = &nodes[neighborIndex];
//             if (neighbor->closed)
//                 continue;

//             Object* obj = map->objects[ny][nx];
//             bool    door = obj && obj->type && obj->type->isDoor && !object_is_walkable(obj);
//             if (door && !(options && options->canOpenDoors))
//                 continue;

//             float stepCost = 1.0f;
//             TileType* tile = get_tile_type(map->tiles[ny][nx]);
//             stepCost *= tile_cost(tile);
//             if (door)
//                 stepCost += 0.5f;

//             float tentativeG = current->g + stepCost;
//             if (!neighbor->open || tentativeG < neighbor->g)
//             {
//                 neighbor->parent = currentIndex;
//                 neighbor->g      = tentativeG;
//                 neighbor->h      = heuristic_cost(nx, ny, gx, gy);

//                 if (!neighbor->open)
//                 {
//                     neighbor->open          = true;
//                     openList[openCount++]   = neighborIndex;
//                 }
//             }
//         }
//     }

//     return false;
// }
#include "pathfinding.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "tile.h"

#define PATHFINDING_MAX_NODES 4096
#define PATHFINDING_MAX_EXTENT 30

typedef struct Node
{
    int            x;
    int            y;
    float          g;
    float          h;
    int            parent;
    bool           open;
    bool           closed;
    unsigned short visitedID;
} Node;

static unsigned short globalVisitID = 1;

// --------------------------------------------------------------------------------------
// Min-heap simple pour la open list
// --------------------------------------------------------------------------------------
typedef struct
{
    int   index;
    float f;
} HeapNode;

typedef struct
{
    HeapNode nodes[PATHFINDING_MAX_NODES];
    int      count;
} MinHeap;

static inline void heap_init(MinHeap* heap)
{
    heap->count = 0;
}

static inline void heap_push(MinHeap* heap, int index, float f)
{
    int i = heap->count++;
    while (i > 0)
    {
        int p = (i - 1) / 2;
        if (heap->nodes[p].f <= f)
            break;
        heap->nodes[i] = heap->nodes[p];
        i              = p;
    }
    heap->nodes[i].index = index;
    heap->nodes[i].f     = f;
}

static inline int heap_pop(MinHeap* heap)
{
    HeapNode root = heap->nodes[0];
    HeapNode last = heap->nodes[--heap->count];
    int      i    = 0;
    while (true)
    {
        int l = 2 * i + 1, r = l + 1;
        if (l >= heap->count)
            break;
        int c = (r < heap->count && heap->nodes[r].f < heap->nodes[l].f) ? r : l;
        if (heap->nodes[c].f >= last.f)
            break;
        heap->nodes[i] = heap->nodes[c];
        i              = c;
    }
    heap->nodes[i] = last;
    return root.index;
}

// --------------------------------------------------------------------------------------
// Heuristique : octile (optimisée pour les 8 directions)
// --------------------------------------------------------------------------------------
static inline float heuristic_cost(int x0, int y0, int x1, int y1)
{
    float dx = (float)abs(x0 - x1);
    float dy = (float)abs(y0 - y1);
    float D  = 1.0f;
    float D2 = 1.41421356f; // sqrt(2)
    // Weighted octile heuristic
    return (D * (dx + dy) + (D2 - 2 * D) * fminf(dx, dy)) * 1.001f;
}

static inline float tile_cost(const TileType* tile)
{
    if (!tile)
        return 1.0f;
    float cost = tile->movementCost;
    return (cost > 0.01f) ? cost : 1.0f;
}

static inline bool tile_walkable_cached(const Map* map, bool** walk, int x, int y)
{
    return (x >= 0 && y >= 0 && x < map->width && y < map->height) ? walk[y][x] : false;
}

static void reconstruct_path(const Node* nodes, int currentIndex, PathfindingPath* outPath)
{
    if (!outPath)
        return;

    Vector2 reverseBuffer[PATHFINDING_MAX_LENGTH];
    int     length = 0;

    while (currentIndex >= 0 && length < PATHFINDING_MAX_LENGTH)
    {
        const Node* n           = &nodes[currentIndex];
        reverseBuffer[length++] = (Vector2){(n->x + 0.5f) * TILE_SIZE, (n->y + 0.5f) * TILE_SIZE};
        currentIndex            = n->parent;
    }

    outPath->count = 0;
    for (int i = length - 1; i >= 0; --i)
        outPath->points[outPath->count++] = reverseBuffer[i];
}

// --------------------------------------------------------------------------------------
// Main Pathfinding avec diagonales
// --------------------------------------------------------------------------------------
bool pathfinding_find_path(const Map* map, Vector2 start, Vector2 goal, const PathfindingOptions* options, PathfindingPath* outPath)
{
    if (outPath)
        memset(outPath, 0, sizeof(*outPath));
    if (!map)
        return false;

    int sx = (int)floorf(start.x / TILE_SIZE);
    int sy = (int)floorf(start.y / TILE_SIZE);
    int gx = (int)floorf(goal.x / TILE_SIZE);
    int gy = (int)floorf(goal.y / TILE_SIZE);

    if (sx == gx && sy == gy)
    {
        if (outPath)
        {
            outPath->points[0] = (Vector2){(gx + 0.5f) * TILE_SIZE, (gy + 0.5f) * TILE_SIZE};
            outPath->count     = 1;
        }
        return true;
    }

    // Pré-calcul du cache de walkables
    bool** walkable = malloc(sizeof(bool*) * map->height);
    for (int y = 0; y < map->height; ++y)
    {
        walkable[y] = malloc(sizeof(bool) * map->width);
        for (int x = 0; x < map->width; ++x)
        {
            TileTypeID tileId = map->tiles[y][x];
            TileType*  tile   = get_tile_type(tileId);
            bool       ok     = (tile && tile->walkable);
            if (ok)
            {
                Object* obj = map->objects[y][x];
                if (obj)
                {
                    if (!object_is_walkable(obj) && !(options && options->canOpenDoors && obj->type && obj->type->isDoor))
                        ok = false;
                }
            }
            walkable[y][x] = ok;
        }
    }

    if (!tile_walkable_cached(map, walkable, sx, sy) || !tile_walkable_cached(map, walkable, gx, gy))
    {
        for (int y = 0; y < map->height; ++y)
            free(walkable[y]);
        free(walkable);
        return false;
    }

    // Définir la zone de recherche
    int halfExtent = PATHFINDING_MAX_EXTENT;
    int minX       = sx < gx ? sx : gx;
    int minY       = sy < gy ? sy : gy;
    int maxX       = sx > gx ? sx : gx;
    int maxY       = sy > gy ? sy : gy;

    while (true)
    {
        int loX = minX - halfExtent, hiX = maxX + halfExtent;
        int loY = minY - halfExtent, hiY = maxY + halfExtent;
        if (loX < 0)
            loX = 0;
        if (loY < 0)
            loY = 0;
        if (hiX >= map->width)
            hiX = map->width - 1;
        if (hiY >= map->height)
            hiY = map->height - 1;

        int width  = hiX - loX + 1;
        int height = hiY - loY + 1;
        if (width * height <= PATHFINDING_MAX_NODES)
        {
            minX = loX;
            maxX = hiX;
            minY = loY;
            maxY = hiY;
            break;
        }
        halfExtent -= 4;
        if (halfExtent <= 4)
        {
            for (int y = 0; y < map->height; ++y)
                free(walkable[y]);
            free(walkable);
            return false;
        }
    }

    int width  = maxX - minX + 1;
    int height = maxY - minY + 1;
    int total  = width * height;

    static Node nodes[PATHFINDING_MAX_NODES];
    ++globalVisitID;

    int startIndex = (sy - minY) * width + (sx - minX);
    int goalIndex  = (gy - minY) * width + (gx - minX);

    Node* startNode      = &nodes[startIndex];
    startNode->x         = sx;
    startNode->y         = sy;
    startNode->g         = 0.0f;
    startNode->h         = heuristic_cost(sx, sy, gx, gy);
    startNode->parent    = -1;
    startNode->open      = true;
    startNode->closed    = false;
    startNode->visitedID = globalVisitID;

    MinHeap heap;
    heap_init(&heap);
    heap_push(&heap, startIndex, startNode->g + startNode->h);

    // 8 directions
    static const int OFFSETS[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

    while (heap.count > 0)
    {
        int   currentIndex = heap_pop(&heap);
        Node* current      = &nodes[currentIndex];
        current->open      = false;
        current->closed    = true;

        if (currentIndex == goalIndex)
        {
            reconstruct_path(nodes, currentIndex, outPath);
            for (int y = 0; y < map->height; ++y)
                free(walkable[y]);
            free(walkable);
            return true;
        }

        for (int n = 0; n < 8; ++n)
        {
            int nx = current->x + OFFSETS[n][0];
            int ny = current->y + OFFSETS[n][1];
            if (!tile_walkable_cached(map, walkable, nx, ny))
                continue;

            // Évite de couper un coin entre deux obstacles
            if (n >= 4)
            {
                int ax = current->x + OFFSETS[n][0];
                int ay = current->y;
                int bx = current->x;
                int by = current->y + OFFSETS[n][1];
                if (!tile_walkable_cached(map, walkable, ax, ay) || !tile_walkable_cached(map, walkable, bx, by))
                    continue;
            }

            int neighborIndex = (ny - minY) * width + (nx - minX);
            if (neighborIndex < 0 || neighborIndex >= total)
                continue;

            Node* neighbor = &nodes[neighborIndex];
            if (neighbor->visitedID != globalVisitID)
            {
                neighbor->x         = nx;
                neighbor->y         = ny;
                neighbor->visitedID = globalVisitID;
                neighbor->g         = FLT_MAX;
                neighbor->h         = heuristic_cost(nx, ny, gx, gy);
                neighbor->parent    = -1;
                neighbor->open      = false;
                neighbor->closed    = false;
            }
            if (neighbor->closed)
                continue;

            float     stepCost = (n < 4) ? 1.0f : 1.41421356f; // diagonale = sqrt(2)
            TileType* tile     = get_tile_type(map->tiles[ny][nx]);
            stepCost *= tile_cost(tile);

            float tentativeG = current->g + stepCost;
            if (!neighbor->open || tentativeG < neighbor->g)
            {
                neighbor->parent = currentIndex;
                neighbor->g      = tentativeG;
                float f          = tentativeG + neighbor->h;
                heap_push(&heap, neighborIndex, f);
                neighbor->open = true;
            }
        }
    }

    for (int y = 0; y < map->height; ++y)
        free(walkable[y]);
    free(walkable);
    return false;
}
