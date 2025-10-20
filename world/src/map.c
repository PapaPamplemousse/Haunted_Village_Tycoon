#include "map.h"

BuildingID grid[MAP_HEIGHT][MAP_WIDTH] = {0}; // 0 = vide, 1 = bâtiment

// Base colour for unoccupied tiles.  This is the standard ground
// colour used for free spaces.
static Color tileColor = {60, 90, 120, 255};

// Slightly darker colour used to tint tiles that are occupied by
// buildings.  This helps the player see which squares have
// structures placed on them.  Feel free to adjust the values to
// taste.
static Color usedTileColor = {40, 60, 80, 255};
// Colour for drawing the grid lines.  Semi‑transparent for a subtle
// effect.
static Color borderColor = {200, 200, 255, 80};

// === Conversion coordonnées ===
static Vector2 to_iso(int x, int y)
{
    return (Vector2){(x - y) * TILE_SIZE / 2, (x + y) * TILE_SIZE / 4};
}

Vector2 world_to_grid(Vector2 worldMouse)
{
    return (Vector2){(worldMouse.y / (TILE_SIZE / 4) + worldMouse.x / (TILE_SIZE / 2)) / 2, (worldMouse.y / (TILE_SIZE / 4) - worldMouse.x / (TILE_SIZE / 2)) / 2};
}

// === Initialisation / Unload ===
void map_init(void)
{
}

void map_unload(void)
{
}

// === Dessin de la carte ===
// void draw_map(Camera2D camera) {
//   for (int y = 0; y < MAP_HEIGHT; y++) {
//     for (int x = 0; x < MAP_WIDTH; x++) {
//       Vector2 pos = to_iso(x, y);

//       Vector2 top = {pos.x + TILE_SIZE / 2, pos.y};
//       Vector2 right = {pos.x + TILE_SIZE, pos.y + TILE_SIZE / 4};
//       Vector2 bottom = {pos.x + TILE_SIZE / 2, pos.y + TILE_SIZE / 2};
//       Vector2 left = {pos.x, pos.y + TILE_SIZE / 4};

//       // Choose the fill colour based on occupancy.  When a tile
//       // contains a building (grid value different from
//       // BUILDING_NONE) we tint it darker to indicate that it is
//       // used.  Otherwise we use the standard tile colour.
//       Color currentColor;
//       if (grid[y][x] != BUILDING_NONE) {
//         currentColor = usedTileColor;
//       } else {
//         currentColor = tileColor;
//       }

//       DrawTriangle(top, right, bottom, currentColor);
//       DrawTriangle(bottom, left, top, currentColor);

//       DrawLineEx(top, right, 1.0f, borderColor);
//       DrawLineEx(right, bottom, 1.0f, borderColor);
//       DrawLineEx(bottom, left, 1.0f, borderColor);
//       DrawLineEx(left, top, 1.0f, borderColor);

//       // if (grid[y][x] == 1) {

//       //   BuildingType *b = get_building_type(get_selected_building());
//       //   if (b) {
//       //     float scale = 0.25f;
//       //     Vector2 iso = to_iso(x, y);
//       //     Vector2 drawPos = {
//       //         iso.x + TILE_SIZE / 2 - (b->texture.width * scale) / 2,
//       //         iso.y + TILE_SIZE / 2 - (b->texture.height * scale)};
//       //     DrawTextureEx(b->texture, drawPos, 0.0f, scale, WHITE);
//       //   }
//       // }
//     }
//   }

//   // === Highlight case survolée ===
//   Vector2 mouse = GetMousePosition();
//   Vector2 worldMouse = GetScreenToWorld2D(mouse, camera);
//   Vector2 gridPos = world_to_grid(worldMouse);

//   int selX = (int)gridPos.x;
//   int selY = (int)gridPos.y;

//   if (selX >= 0 && selX < MAP_WIDTH && selY >= 0 && selY < MAP_HEIGHT) {
//     Vector2 pos = to_iso(selX, selY);
//     Vector2 top = {pos.x + TILE_SIZE / 2, pos.y};
//     Vector2 right = {pos.x + TILE_SIZE, pos.y + TILE_SIZE / 4};
//     Vector2 bottom = {pos.x + TILE_SIZE / 2, pos.y + TILE_SIZE / 2};
//     Vector2 left = {pos.x, pos.y + TILE_SIZE / 4};

//     DrawLineEx(top, right, 2.0f, YELLOW);
//     DrawLineEx(right, bottom, 2.0f, YELLOW);
//     DrawLineEx(bottom, left, 2.0f, YELLOW);
//     DrawLineEx(left, top, 2.0f, YELLOW);

//     // Aperçu bâtiment semi-transparent (bien aligné sur la grille)
//     Vector2 gridPos = {selX, selY};
//     draw_building_preview(gridPos);
//   }

//   for (int y = 0; y < MAP_HEIGHT; y++) {
//     for (int x = 0; x < MAP_WIDTH; x++) {
//       BuildingID id = grid[y][x];
//       if (id != BUILDING_NONE) {
//         // Vérifier que cette case est le coin haut-gauche du bâtiment (pour
//         ne
//         // pas le dessiner plusieurs fois)
//         if ((y == 0 || grid[y - 1][x] != id) &&
//             (x == 0 || grid[y][x - 1] != id)) {
//           BuildingType *bType = get_building_type(id);
//           // Calculer la position écran isométrique de la tuile (x,y)
//           float screenX =
//               (float)(x - y) * (TILE_WIDTH / 2) + /* décalage X de la map */
//               0;
//           float screenY =
//               (float)(x + y) * (TILE_HEIGHT / 2) + /* décalage Y de la map */
//               0;
//           // Dessiner la texture du bâtiment à cette position.
//           // Si le bâtiment occupe plusieurs tuiles en hauteur, on peut
//           ajuster
//           // la position Y pour bien caler sa base.
//           DrawTexture(bType->texture, (int)screenX, (int)screenY, WHITE);
//         }
//       }
//     }
//   }
// }
void draw_map(Camera2D camera)
{
    // -- 1. Dessin des tuiles
    for (int y = 0; y < MAP_HEIGHT; y++)
    {
        for (int x = 0; x < MAP_WIDTH; x++)
        {
            Vector2 pos    = to_iso(x, y);
            Vector2 top    = {pos.x + TILE_SIZE / 2, pos.y};
            Vector2 right  = {pos.x + TILE_SIZE, pos.y + TILE_SIZE / 4};
            Vector2 bottom = {pos.x + TILE_SIZE / 2, pos.y + TILE_SIZE / 2};
            Vector2 left   = {pos.x, pos.y + TILE_SIZE / 4};

            Color currentColor = (grid[y][x] != BUILDING_NONE) ? usedTileColor : tileColor;

            DrawTriangle(top, right, bottom, currentColor);
            DrawTriangle(bottom, left, top, currentColor);

            DrawLineEx(top, right, 1.0f, borderColor);
            DrawLineEx(right, bottom, 1.0f, borderColor);
            DrawLineEx(bottom, left, 1.0f, borderColor);
            DrawLineEx(left, top, 1.0f, borderColor);
        }
    }

    // -- 2. Highlight case survolée
    Vector2 mouse      = GetMousePosition();
    Vector2 worldMouse = GetScreenToWorld2D(mouse, camera);
    Vector2 gridPos    = world_to_grid(worldMouse);

    int selX = (int)gridPos.x;
    int selY = (int)gridPos.y;

    if (selX >= 0 && selX < MAP_WIDTH && selY >= 0 && selY < MAP_HEIGHT)
    {
        Vector2 pos    = to_iso(selX, selY);
        Vector2 top    = {pos.x + TILE_SIZE / 2, pos.y};
        Vector2 right  = {pos.x + TILE_SIZE, pos.y + TILE_SIZE / 4};
        Vector2 bottom = {pos.x + TILE_SIZE / 2, pos.y + TILE_SIZE / 2};
        Vector2 left   = {pos.x, pos.y + TILE_SIZE / 4};

        DrawLineEx(top, right, 2.0f, YELLOW);
        DrawLineEx(right, bottom, 2.0f, YELLOW);
        DrawLineEx(bottom, left, 2.0f, YELLOW);
        DrawLineEx(left, top, 2.0f, YELLOW);

        // Aperçu semi-transparent
        draw_building_preview((Vector2){selX, selY});
    }

    // -- 3. Affichage des bâtiments
    for (int y = 0; y < MAP_HEIGHT; y++)
    {
        for (int x = 0; x < MAP_WIDTH; x++)
        {
            BuildingID id = grid[y][x];
            if (id != BUILDING_NONE)
            {
                // Ne dessiner qu’une seule fois le bâtiment
                if ((y == 0 || grid[y - 1][x] != id) && (x == 0 || grid[y][x - 1] != id))
                {
                    BuildingType* type = get_building_type(id);
                    if (type)
                    {
                        Vector2 iso     = to_iso(x, y);
                        Vector2 drawPos = {iso.x + TILE_SIZE / 2 - (type->texture.width * type->scale) / 2, iso.y + TILE_SIZE / 2 - (type->texture.height * type->scale)};
                        DrawTextureEx(type->texture, drawPos, 0.0f, type->scale, WHITE);
                    }
                }
            }
        }
    }
}

// === Mise à jour de la carte ===
// void update_map(Camera2D camera) {
//   Vector2 mouse = GetMousePosition();
//   Vector2 worldMouse = GetScreenToWorld2D(mouse, camera);
//   Vector2 gridPos = world_to_grid(worldMouse);

//   int gx = (int)gridPos.x;
//   int gy = (int)gridPos.y;

//   if (gx >= 0 && gx < MAP_WIDTH && gy >= 0 && gy < MAP_HEIGHT) {
//     if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
//       Vector2 mouse = GetMousePosition();
//       Vector2 worldMouse = GetScreenToWorld2D(mouse, camera);
//       Vector2 gridPos = world_to_grid(worldMouse);
//       gridPos.x = (int)gridPos.x;
//       gridPos.y = (int)gridPos.y;
//       place_building(gridPos);
//     } else if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
//       grid[gy][gx] = 0;
//     }
//   }
// }

void update_map(Camera2D camera)
{
    Vector2 mouse      = GetMousePosition();
    Vector2 worldMouse = GetScreenToWorld2D(mouse, camera);
    Vector2 gridPos    = world_to_grid(worldMouse);

    int gx = (int)gridPos.x;
    int gy = (int)gridPos.y;

    if (gx >= 0 && gx < MAP_WIDTH && gy >= 0 && gy < MAP_HEIGHT)
    {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            place_building((Vector2){gx, gy});
        }
        else if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
        {
            grid[gy][gx] = BUILDING_NONE;
        }
    }
}

// void occupy_tiles(Vector2 origin, BuildingID id) {
//   BuildingType *type = get_building_type(id);
//   int startX = (int)origin.x;
//   int startY = (int)origin.y;
//   // Marquer chaque case de la zone occupée par le bâtiment
//   for (int dy = 0; dy < type->height; dy++) {
//     for (int dx = 0; dx < type->width; dx++) {
//       // Assurer que l'on reste dans les limites de la grille
//       if (startY + dy < MAP_HEIGHT && startX + dx < MAP_WIDTH) {
//         grid[startY + dy][startX + dx] = id;
//       }
//     }
//   }
// }
void occupy_tiles(Vector2 origin, BuildingID id)
{
    BuildingType* type = get_building_type(id);
    if (!type)
        return;

    int startX = (int)origin.x;
    int startY = (int)origin.y;

    for (int dy = 0; dy < type->height; dy++)
    {
        for (int dx = 0; dx < type->width; dx++)
        {
            int tx = startX + dx;
            int ty = startY + dy;
            if (tx >= 0 && tx < MAP_WIDTH && ty >= 0 && ty < MAP_HEIGHT)
            {
                grid[ty][tx] = id;
            }
        }
    }
}
