#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "building.h"
#include "tile.h"
#include "object.h"

typedef struct
{
    int       area;
    Rectangle bounds;
    int       doorCount;
    int       wallBoundaryCount;
    bool      nonStructuralBlocker; // e.g. bed / table used as boundary -> invalidates
    bool      touchesBorder;
} FloodResult;

Building buildings[MAX_BUILDINGS];
int      buildingCount = 0;

// ============================
// Helpers
// ============================

static inline bool is_non_structural_blocker(const Object* obj)
{
    if (!obj)
        return false;

    // Si c'est une porte -> pas bloquant structurel (on la compte ailleurs)
    if (is_door_object(obj))
        return false;

    // Si c'est un mur -> structurel OK (on le compte ailleurs)
    if (is_wall_object(obj))
        return false;

    // Tout objet non-walkable (lit, table, coffre, etc.) est un "blocage non structurel".
    return !obj->type->walkable;
}

static FloodResult perform_flood_fill(int sx, int sy, bool visited[MAP_HEIGHT][MAP_WIDTH])
{
    FloodResult res = {0};

    int minx = G_MAP.width, miny = G_MAP.height;
    int maxx = -1, maxy = -1;

    // Pile pour BFS/DFS itératif
    const int stackCap = G_MAP.width * G_MAP.height;
    int*      stack    = (int*)malloc(stackCap * sizeof(int));
    int       top      = 0;

    // On ne démarre QUE depuis du sol vide
    visited[sy][sx] = true;
    stack[top++]    = sy * G_MAP.width + sx;

    while (top > 0)
    {
        const int idx = stack[--top];
        const int cx  = idx % G_MAP.width;
        const int cy  = idx / G_MAP.width;

        // Comptage/borne
        res.area++;
        if (cx < minx)
            minx = cx;
        if (cy < miny)
            miny = cy;
        if (cx > maxx)
            maxx = cx;
        if (cy > maxy)
            maxy = cy;

        // 4-connexité, SANS wrap (si on sort -> touche bord)
        static const int DIRS[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
        for (int d = 0; d < 4; ++d)
        {
            const int nx = cx + DIRS[d][0];
            const int ny = cy + DIRS[d][1];

            if (nx < 0 || ny < 0 || nx >= G_MAP.width || ny >= G_MAP.height)
            {
                res.touchesBorder = true;
                continue;
            }

            if (visited[ny][nx])
                continue;

            Object* obj = G_MAP.objects[ny][nx];

            if (!obj)
            {
                // 1. Sol vide
                visited[ny][nx] = true;
                stack[top++]    = ny * G_MAP.width + nx;
            }
            else if (is_door_object(obj))
            {
                // 2. Porte (Structurel, compté, non bloquant pour le flood)
                res.doorCount++;
                continue;
            }
            else if (is_wall_object(obj))
            {
                // 3. Mur (Structurel, compté, bloquant pour le flood)
                res.wallBoundaryCount++;
                continue;
            }
            else if (is_non_structural_blocker(obj)) // <= LE LIT
            {
                // 4. Meuble non-structurel non-walkable : INVALIDE LA PIÈCE
                visited[ny][nx] = true;
                stack[top++]    = ny * G_MAP.width + nx;
            }
            else
            {
                // 5. Objet walkable (plante, torche, etc.)
                visited[ny][nx] = true;
                stack[top++]    = ny * G_MAP.width + nx;
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

static bool is_valid_building_area(const FloodResult* r)
{
    // Conditions minimales :
    // - ne pas être ouverte sur le bord
    // - contenir au moins une porte
    // - avoir été bordée par au moins un mur structurel (sinon "clôture" en meubles)
    // - aucun "nonStructuralBlocker" en frontière (lit/table utilisés comme murs => interdit)
    // - surface > 0
    bool ret = true;

    if ((r->touchesBorder) || (r->nonStructuralBlocker) || (r->wallBoundaryCount <= 0) || (r->doorCount <= 0) || (r->area <= 0))
    {
        ret = false;
    }

    return ret;
}

static void init_building_structure(Building* b, int id, const FloodResult* res)
{
    b->id     = id;
    b->bounds = res->bounds;
    b->area   = res->area;
    b->center = (Vector2){res->bounds.x + res->bounds.width / 2.0f, res->bounds.y + res->bounds.height / 2.0f};

    // Initialisation temporaire
    b->objectCount = 0;
    b->objects     = NULL;
    b->roomType    = NULL; // Sera mis à jour après analyse
    // Les champs b->name seront initialisés après l'analyse
}

static void collect_building_objects(Building* b, const FloodResult* res, const bool visited[MAP_HEIGHT][MAP_WIDTH])
{
    // On alloue un espace maximal (la taille de la zone inondée)
    Object** temp_objects    = (Object**)malloc(res->area * sizeof(Object*));
    int      collected_count = 0;

    // Parcourir la Bounding Box détectée par le flood
    for (int y = (int)res->bounds.y; y < res->bounds.y + res->bounds.height; ++y)
    {
        for (int x = (int)res->bounds.x; x < res->bounds.x + res->bounds.width; ++x)
        {
            // 1. On vérifie que cette coordonnée fait bien partie de la zone inondée
            //    (ce qui est garanti par l'array 'visited' après le flood)
            if (!visited[y][x])
                continue;

            Object* obj = G_MAP.objects[y][x];

            // 2. On collecte uniquement les objets qui ne sont PAS des murs structurels ni des portes.
            //    (Les meubles comme le lit, la table, les décorations, etc., sont inclus ici.)
            if (obj && !is_wall_object(obj) && !is_door_object(obj))
            {
                temp_objects[collected_count++] = obj;
            }
        }
    }

    // 3. Finaliser l'assignation des objets à la structure Building
    b->objectCount = collected_count;
    if (collected_count > 0)
    {
        // Allouer la taille exacte et copier les pointeurs
        b->objects = (Object**)malloc(collected_count * sizeof(Object*));
        memcpy(b->objects, temp_objects, collected_count * sizeof(Object*));
    }
    else
    {
        b->objects = NULL;
    }

    free(temp_objects); // Libérer l'allocation temporaire
}

// ============================
// API
// ============================

void update_building_detection(void)
{
    buildingCount = 0;

    // Utilisation de static pour éviter la réallocation, mais memset pour la réinitialisation
    static bool visited[MAP_HEIGHT][MAP_WIDTH];
    memset(visited, 0, sizeof(visited));

    int nextId = 1;

    // --- 1. BALAYAGE DE LA CARTE ---
    for (int y = 0; y < G_MAP.height; ++y)
    {
        for (int x = 0; x < G_MAP.width; ++x)
        {
            if (visited[y][x])
                continue;

            Object* obj = G_MAP.objects[y][x];

            // Ne JAMAIS démarrer sur un objet bloquant, mais ne marquer que les structurels comme visited.
            if (obj)
            {
                // Murs et Portes (bloqueurs structurels)
                if (is_wall_object(obj) || is_door_object(obj))
                {
                    visited[y][x] = true; // On les marque pour ne pas les revisiter inutilement
                    continue;
                }
                // Meubles non-walkable (Lit, Table, etc.)
                if (is_non_structural_blocker(obj))
                {
                    // Ne PAS marquer comme visited! Le flood doit le rencontrer pour valider/invalider.
                    continue;
                }
                // Les autres objets walkable (Torches, Plantes) sont autorisés comme point de départ.
            }

            // --- 2. FLOOD-FILL ---
            // Si c'est du sol vide OU un objet walkable : flood-fill
            FloodResult res = perform_flood_fill(x, y, visited);

            // --- 3. VALIDATION ---
            if (!is_valid_building_area(&res))
                continue;
            if (buildingCount >= MAX_BUILDINGS)
                continue;

            // --- 4. INITIALISATION ET COLLECTE ---
            Building* b = &buildings[buildingCount];

            // Initialisation des champs de base
            init_building_structure(b, nextId, &res);

            // Collecte des objets intérieurs (correction du bug du Object Count = 0)
            collect_building_objects(b, &res, visited);

            buildingCount++;
            nextId++;

            // --- 5. CLASSIFICATION ---
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
        }
    }
}
