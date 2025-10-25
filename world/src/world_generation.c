#include "world_generation.h"
#include "tile.h"
#include "object.h"
#include "map.h"
#include <math.h>
#include <string.h> // memset
#include "world_structures.h"
#include <stdlib.h>
#include <stdio.h>
#include "world_chunk.h"

typedef struct
{
    int x, y;
} P2i;

// ---------------- RNG déterministe (splitmix64 + xorshift32) ----------------
static uint64_t g_seed64 = 0x12345678ABCDEF01ull;

static uint32_t xorshift32(uint32_t* s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}
static uint64_t splitmix64_next(uint64_t* x)
{
    uint64_t z = (*x += 0x9E3779B97F4A7C15ull);
    z          = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z          = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
static float rng01(uint64_t* s)
{
    return (splitmix64_next(s) >> 8) * (1.0f / (float)(1ull << 56));
}

// ------------------- Bruits légers (value noise + fBm) ----------------------
static float hash2i(int x, int y, uint32_t salt)
{
    uint32_t h = 2166136261u ^ (uint32_t)x;
    h          = (h ^ (uint32_t)y) * 16777619u;
    h ^= salt * 374761393u;
    return (h & 0x00FFFFFF) / 16777215.0f; // [0,1]
}
static float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}
static float smooth(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

// value noise bilinéaire
static float value2D(float x, float y, uint32_t salt)
{
    int   xi = (int)floorf(x), yi = (int)floorf(y);
    float xf = x - xi, yf = y - yi;
    float v00 = hash2i(xi, yi, salt), v10 = hash2i(xi + 1, yi, salt);
    float v01 = hash2i(xi, yi + 1, salt), v11 = hash2i(xi + 1, yi + 1, salt);
    float u = smooth(xf), v = smooth(yf);
    return lerp(lerp(v00, v10, u), lerp(v01, v11, u), v);
}

// fBm (fractal Brownian motion)
static float fbm2D(float x, float y, int octaves, float lac, float gain, float baseFreq, uint32_t salt)
{
    float amp = 1.0f, freq = baseFreq, sum = 0.0f, norm = 0.0f;
    for (int i = 0; i < octaves; i++)
    {
        sum += value2D(x * freq, y * freq, salt + i * 97u) * amp;
        norm += amp;
        amp *= gain;
        freq *= lac;
    }
    return sum / (norm > 0 ? norm : 1.0f); // [0,1]
}

// domain warping léger pour de plus gros "blobs"
static float warped2D(float x, float y, uint32_t salt)
{
    float dx = fbm2D(x + 13.7f, y - 9.1f, 3, 2.0f, 0.5f, 0.005f, 1337u ^ salt);
    float dy = fbm2D(x - 4.2f, y + 7.3f, 3, 2.0f, 0.5f, 0.005f, 7331u ^ salt);
    return fbm2D(x + (dx - 0.5f) * 150.0f, y + (dy - 0.5f) * 150.0f, 4, 2.0f, 0.5f, 0.0025f, 4242u ^ salt);
}

P2i placed[1024];
int placedCount = 0;

// ---------------------- DEFAULT / Config & GLOBAL PARAMETERS --------------------------
static WorldGenParams g_cfg = {
    .min_biome_radius           = (MAP_WIDTH + MAP_HEIGHT) / 16,
    .weight_forest              = 1.0f,
    .weight_plain               = 1.0f,
    .weight_savanna             = 0.8f,
    .weight_tundra              = 0.6f,
    .weight_desert              = 0.7f,
    .weight_swamp               = 0.5f,
    .weight_mountain            = 0.4f,
    .weight_cursed              = 0.10f,
    .weight_hell                = 0.05f,
    .feature_density            = 0.08f,   // densité de décors
    .structure_chance           = 0.0003f, // chance par tuile d’ancrer une structure
    .structure_min_spacing      = (MAP_WIDTH + MAP_HEIGHT) / 32,
    .biome_struct_mult_forest   = 0.4f,
    .biome_struct_mult_plain    = 1.0f,
    .biome_struct_mult_savanna  = 1.2f,
    .biome_struct_mult_tundra   = 0.5f,
    .biome_struct_mult_desert   = 0.3f,
    .biome_struct_mult_swamp    = 0.6f,
    .biome_struct_mult_mountain = 0.4f,
    .biome_struct_mult_cursed   = 0.8f,
    .biome_struct_mult_hell     = 0.1f,
};

void worldgen_seed(uint64_t seed)
{
    g_seed64 = seed ? seed : 0xDEADBEEFCAFEBEEFull;
}
void worldgen_config(const WorldGenParams* params)
{
    if (params)
        g_cfg = *params;
}

static TileTypeID t_forest_primary(void)
{
    return TILE_FOREST;
}
static TileTypeID t_forest_secondary(void)
{
    return TILE_GRASS;
}
static TileTypeID t_plain_primary(void)
{
    return TILE_PLAIN;
}
static TileTypeID t_plain_secondary(void)
{
    return TILE_GRASS;
}
static TileTypeID t_savanna_primary(void)
{
    return TILE_SAVANNA;
}
static TileTypeID t_savanna_secondary(void)
{
    return TILE_PLAIN;
}
static TileTypeID t_tundra_primary(void)
{
    return TILE_TUNDRA;
}
static TileTypeID t_tundra_secondary(void)
{
    return TILE_PLAIN;
}
static TileTypeID t_desert_primary(void)
{
    return TILE_DESERT;
}
static TileTypeID t_desert_secondary(void)
{
    return TILE_SAVANNA;
}
static TileTypeID t_swamp_primary(void)
{
    return TILE_SWAMP;
}
static TileTypeID t_swamp_secondary(void)
{
    return TILE_FOREST;
}
static TileTypeID t_mountain_primary(void)
{
    return TILE_MOUNTAIN;
}
static TileTypeID t_mountain_secondary(void)
{
    return TILE_PLAIN;
}
static TileTypeID t_cursed_primary(void)
{
    return TILE_CURSED_FOREST;
}
static TileTypeID t_cursed_secondary(void)
{
    return TILE_FOREST;
}
static TileTypeID t_hell_primary(void)
{
    return TILE_HELL;
}
static TileTypeID t_hell_secondary(void)
{
    return TILE_LAVA;
}

static BiomeKind pick_biome(uint64_t* rs, float humid, float tempN)
{
    // pondérations, avec garde-fous climatiques simples
    float wf = g_cfg.weight_forest * (humid > 0.55f);
    float wp = g_cfg.weight_plain * (humid > 0.30f);
    float ws = g_cfg.weight_savanna * (humid > 0.15f && tempN > 0.45f);
    float wt = g_cfg.weight_tundra * (tempN < 0.35f);
    float wd = g_cfg.weight_desert * (humid < 0.20f && tempN > 0.55f);
    float ww = g_cfg.weight_swamp * (humid > 0.7f && tempN > 0.35f);
    float wm = g_cfg.weight_mountain * 1.0f;
    float wc = g_cfg.weight_cursed * (humid > 0.4f);
    float wh = g_cfg.weight_hell * (tempN > 0.8f);

    float sum = wf + wp + ws + wt + wd + ww + wm + wc + wh;
    if (sum <= 0.0f)
    {
        wp  = 1.0f;
        sum = 1.0f;
    } // fallback
    float r = rng01(rs) * sum;

#define PULL(acc, w, kind)                                                                                                                                                                                                 \
    do                                                                                                                                                                                                                     \
    {                                                                                                                                                                                                                      \
        if ((acc) + (w) >= r)                                                                                                                                                                                              \
            return kind;                                                                                                                                                                                                   \
        acc += (w);                                                                                                                                                                                                        \
    } while (0)
    float a = 0;
    PULL(a, wf, BIO_FOREST);
    PULL(a, wp, BIO_PLAIN);
    PULL(a, ws, BIO_SAVANNA);
    PULL(a, wt, BIO_TUNDRA);
    PULL(a, wd, BIO_DESERT);
    PULL(a, ww, BIO_SWAMP);
    PULL(a, wm, BIO_MOUNTAIN);
    PULL(a, wc, BIO_CURSED);
    return BIO_HELL;
#undef PULL
}

static void biome_kinds_to_tiles(BiomeKind k, TileTypeID* p, TileTypeID* s)
{
    switch (k)
    {
        case BIO_FOREST:
            *p = t_forest_primary();
            *s = t_forest_secondary();
            break;
        case BIO_PLAIN:
            *p = t_plain_primary();
            *s = t_plain_secondary();
            break;
        case BIO_SAVANNA:
            *p = t_savanna_primary();
            *s = t_savanna_secondary();
            break;
        case BIO_TUNDRA:
            *p = t_tundra_primary();
            *s = t_tundra_secondary();
            break;
        case BIO_DESERT:
            *p = t_desert_primary();
            *s = t_desert_secondary();
            break;
        case BIO_SWAMP:
            *p = t_swamp_primary();
            *s = t_swamp_secondary();
            break;
        case BIO_MOUNTAIN:
            *p = t_mountain_primary();
            *s = t_mountain_secondary();
            break;
        case BIO_CURSED:
            *p = t_cursed_primary();
            *s = t_cursed_secondary();
            break;
        case BIO_HELL:
            *p = t_hell_primary();
            *s = t_hell_secondary();
            break;
    }
}

// Poisson-disk approximatif: accepte un centre si éloigné des précédents
static int spawn_biome_centers(BiomeCenter* out, int maxN, int W, int H, int minDist, uint64_t* rs)
{
    int n = 0;
    // densité approx : surface / (pi r^2 * k)
    float approx = (float)(W * H) / (3.14159f * minDist * minDist * 1.2f);
    int   target = (int)fmaxf(4.0f, fminf((float)maxN, approx));

    for (int tries = 0; tries < target * 40 && n < target; ++tries)
    {
        int x = (int)(rng01(rs) * W);
        int y = (int)(rng01(rs) * H);

        int ok = 1;
        for (int i = 0; i < n; i++)
        {
            int dx = x - out[i].x, dy = y - out[i].y;
            if (dx * dx + dy * dy < minDist * minDist)
            {
                ok = 0;
                break;
            }
        }
        if (!ok)
            continue;

        // climat local pour pondérer le choix
        float lat   = (float)y / (float)H;        // 0 nord -> 1 sud
        float tempN = 0.5f + 0.4f * (0.5f - lat); // plus froid au nord
        tempN       = fminf(1.0f, fmaxf(0.0f, tempN));
        float humid = warped2D((float)x, (float)y, 1111u); // [0..1]

        BiomeKind  k = pick_biome(rs, humid, tempN);
        TileTypeID p, s;
        biome_kinds_to_tiles(k, &p, &s);

        out[n++] = (BiomeCenter){.x = x, .y = y, .kind = k, .primary = p, .secondary = s};
    }
    return n;
}

// trouve l’index du centre le plus proche (naïf, suffisant à la génération)
static int nearest_center(const BiomeCenter* arr, int n, int x, int y)
{
    int best  = -1;
    int bestd = 0x7FFFFFFF;
    for (int i = 0; i < n; i++)
    {
        int dx = x - arr[i].x, dy = y - arr[i].y;
        int d = dx * dx + dy * dy;
        if (d < bestd)
        {
            bestd = d;
            best  = i;
        }
    }
    return best;
}

// ------------------------- Placement d’objets & structures ------------------
// Helpers denses mais sûrs: check bounds & occupation
static inline int in_bounds(int x, int y, int W, int H)
{
    return (x >= 0 && y >= 0 && x < W && y < H);
}

static void maybe_place_object(Map* map, int x, int y, ObjectTypeID oid, float prob, uint64_t* rs)
{
    if (!in_bounds(x, y, map->width, map->height))
        return;
    if (map->objects[y][x] != NULL)
        return;
    if (rng01(rs) < prob)
    {
        map_place_object(map, oid, x, y);
    }
}

static void generate_lakes(Map* map, uint64_t* rng)
{
    const int W = map->width;
    const int H = map->height;

    // Nombre moyen de lacs selon la taille de carte
    int lakeCount = (W * H) / 4000; // 1 lac / 4000 tuiles environ

    for (int i = 0; i < lakeCount; i++)
    {
        bool isLava = (rng01(rng) < 0.2f); // 20 % de lacs de lave
        int  cx     = rand() % W;
        int  cy     = rand() % H;
        int  rx     = 4 + rand() % 8; // rayon X
        int  ry     = 3 + rand() % 6; // rayon Y

        TileTypeID centerType    = map->tiles[cy][cx];
        float      biomeLakeMult = 1.0f;
        switch (centerType)
        {
            case TILE_TUNDRA:
                biomeLakeMult = 1.5f;
                break; // plus d’eau
            case TILE_DESERT:
                biomeLakeMult = 0.5f;
                break; // peu d’eau
            case TILE_HELL:
                biomeLakeMult = 1.3f;
                break; // mares de lave
            case TILE_SWAMP:
                biomeLakeMult = 2.0f;
                break; // très humide
            default:
                break;
        }

        // Ajuste la probabilité de création selon le biome
        if (rng01(rng) > 0.5f * biomeLakeMult)
            continue; // ne crée rien ici

        TileTypeID fill = isLava ? TILE_LAVA : TILE_WATER;

        // Vérifie compatibilité biome / type de lac
        if (isLava)
        {
            if (!(centerType == TILE_HELL || centerType == TILE_MOUNTAIN || centerType == TILE_DESERT))
                continue;
        }
        else
        {
            if (!(centerType == TILE_PLAIN || centerType == TILE_FOREST || centerType == TILE_SAVANNA || centerType == TILE_TUNDRA || centerType == TILE_SWAMP))
                continue;
        }

        // Dessine l’ellipse
        // for (int y = cy - ry; y <= cy + ry; y++)
        // {
        //     if (y < 0 || y >= H)
        //         continue;
        //     for (int x = cx - rx; x <= cx + rx; x++)
        //     {
        //         if (x < 0 || x >= W)
        //             continue;
        //         float dx = (float)(x - cx) / rx;
        //         float dy = (float)(y - cy) / ry;
        //         if (dx * dx + dy * dy <= 1.0f)
        //         {
        //             map->tiles[y][x]   = fill;
        //             map->objects[y][x] = NULL;
        //         }
        //     }
        // }
        // Draw the ellipse (lake area)
        for (int y = cy - ry; y <= cy + ry; y++)
        {
            if (y < 0 || y >= H)
                continue;
            for (int x = cx - rx; x <= cx + rx; x++)
            {
                if (x < 0 || x >= W)
                    continue;
                float dx = (float)(x - cx) / rx;
                float dy = (float)(y - cy) / ry;
                if (dx * dx + dy * dy <= 1.0f)
                {
                    map->tiles[y][x]   = fill;
                    map->objects[y][x] = NULL;
                }
            }
        }

        // ✅ Tell the chunk system that this lake region changed
        int x0 = cx - rx;
        int y0 = cy - ry;
        int w  = rx * 2 + 1;
        int h  = ry * 2 + 1;
        if (x0 < 0)
        {
            w += x0;
            x0 = 0;
        }
        if (y0 < 0)
        {
            h += y0;
            y0 = 0;
        }
        // chunkgrid_mark_dirty_rect(gChunks, (Rectangle){(float)x0, (float)y0, (float)w, (float)h});
    }
}

// ----------------------------- Génération complète --------------------------
void generate_world(Map* map)
{
    if (!map)
        return;
    const int W = map->width, H = map->height;

    // 1) Génère cartes climatiques “douces”
    //    - heightMask crée mers/laves extrêmes
    //    - humid/temp guident les biomes
    float heightMask, humid, tempN;

    // 2) Centres de biomes (grosses régions)
    const int   MAXC = 512;
    BiomeCenter centers[MAXC];
    uint64_t    rs   = g_seed64;
    int         minR = g_cfg.min_biome_radius;
    int         nC   = spawn_biome_centers(centers, MAXC, W, H, minR, &rs);

    // 3) Assigne chaque case au centre le plus proche -> tuile primaire/secondaire
    for (int y = 0; y < H; y++)
    {
        for (int x = 0; x < W; x++)
        {
            // clim
            float lat  = (float)y / (float)H;
            tempN      = 0.5f + 0.4f * (0.5f - lat); // gradient nord/sud
            tempN      = fminf(1.0f, fmaxf(0.0f, tempN));
            float wv   = warped2D((float)x, (float)y, 9101u);
            humid      = 0.15f + 0.7f * wv;                   // [~0.15..0.85]
            heightMask = warped2D(x * 0.7f, y * 0.7f, 2718u); // [0..1]

            // mers / lave “continents”
            if (heightMask < 0.06f)
            {
                map->tiles[y][x]   = TILE_WATER;
                map->objects[y][x] = NULL;
                continue;
            }
            if (heightMask > 0.97f)
            {
                map->tiles[y][x]   = TILE_LAVA;
                map->objects[y][x] = NULL;
                continue;
            }

            // région de biome
            int        ci   = nearest_center(centers, nC, x, y);
            TileTypeID prim = centers[ci].primary;
            TileTypeID sec  = centers[ci].secondary;

            // légère texture interne (taches) avec bruit HF
            float hf         = fbm2D(x, y, 3, 2.1f, 0.5f, 0.05f, 888u);
            map->tiles[y][x] = (hf > 0.45f) ? prim : sec;

            map->objects[y][x] = NULL;

            // 4) Décors selon biome (probabilité contrôlée)
            float fd = g_cfg.feature_density;
            switch (centers[ci].kind)
            {
                case BIO_FOREST:
                    maybe_place_object(map, x, y, OBJ_TREE, fd * 1.5f, &rs);
                    maybe_place_object(map, x, y, OBJ_STDBUSH, fd * 0.6f, &rs);
                    break;
                case BIO_PLAIN:
                    maybe_place_object(map, x, y, OBJ_STDBUSH, fd * 0.3f, &rs);
                    break;
                case BIO_SAVANNA:
                    maybe_place_object(map, x, y, OBJ_STDBUSH_DRY, fd * 0.8f, &rs);
                    maybe_place_object(map, x, y, OBJ_ROCK, fd * 0.2f, &rs);
                    break;
                case BIO_TUNDRA:
                    maybe_place_object(map, x, y, OBJ_DEAD_TREE, fd * 0.5f, &rs);
                    maybe_place_object(map, x, y, OBJ_ROCK, fd * 0.4f, &rs);
                    break;
                case BIO_DESERT:
                    maybe_place_object(map, x, y, OBJ_ROCK, fd * 0.7f, &rs);
                    break;
                case BIO_SWAMP:
                    maybe_place_object(map, x, y, OBJ_TREE, fd * 0.6f, &rs);
                    maybe_place_object(map, x, y, OBJ_STDBUSH, fd * 0.5f, &rs);
                    break;
                case BIO_MOUNTAIN:
                    maybe_place_object(map, x, y, OBJ_ROCK, fd * 1.2f, &rs);
                    break;
                case BIO_CURSED:
                    maybe_place_object(map, x, y, OBJ_DEAD_TREE, fd * 1.2f, &rs);
                    break;
                case BIO_HELL:
                    maybe_place_object(map, x, y, OBJ_SULFUR_VENT, fd * 0.3f, &rs);
                    break;
            }
        }
    }

    /* generate lakes*/
    generate_lakes(map, &rs);

    for (int y = 2; y < H - 10; y++)
    {
        for (int x = 2; x < W - 10; x++)
        {
            int       ci   = nearest_center(centers, nC, x, y);
            BiomeKind kind = centers[ci].kind;

            // Multiplicateur par biome (déjà dans ta config)
            float biomeMult = 1.0f;
            switch (kind)
            {
                case BIO_FOREST:
                    biomeMult = g_cfg.biome_struct_mult_forest;
                    break;
                case BIO_PLAIN:
                    biomeMult = g_cfg.biome_struct_mult_plain;
                    break;
                case BIO_SAVANNA:
                    biomeMult = g_cfg.biome_struct_mult_savanna;
                    break;
                case BIO_TUNDRA:
                    biomeMult = g_cfg.biome_struct_mult_tundra;
                    break;
                case BIO_DESERT:
                    biomeMult = g_cfg.biome_struct_mult_desert;
                    break;
                case BIO_SWAMP:
                    biomeMult = g_cfg.biome_struct_mult_swamp;
                    break;
                case BIO_MOUNTAIN:
                    biomeMult = g_cfg.biome_struct_mult_mountain;
                    break;
                case BIO_CURSED:
                    biomeMult = g_cfg.biome_struct_mult_cursed;
                    break;
                case BIO_HELL:
                    biomeMult = g_cfg.biome_struct_mult_hell;
                    break;
            }

            float finalChance = g_cfg.structure_chance * biomeMult;
            if (((float)rand() / RAND_MAX) < finalChance)
            {
                printf("[WORLDGEN] Trying to spawn structure at %d,%d (biome %d)\n", x, y, kind);

                // Espacement minimal entre ancres
                int ok = 1;
                for (int i = 0; i < placedCount; i++)
                {
                    int dx = x - placed[i].x;
                    int dy = y - placed[i].y;
                    if (dx * dx + dy * dy < g_cfg.structure_min_spacing * g_cfg.structure_min_spacing)
                    {
                        ok = 0;
                        break;
                    }
                }
                if (!ok)
                    continue;

                // Sélection data-driven de la structure adaptée au biome
                const StructureDef* def = pick_structure_for_biome(kind, &rs);
                if (def)
                {
                    printf("[WORLDGEN] Picked structure: %s\n", def->name);
                    def->build(map, x, y, &rs); // Enregistre automatiquement le Building
                    if (placedCount < (int)(sizeof(placed) / sizeof(placed[0])))
                        placed[placedCount++] = (P2i){x, y};
                }
                else
                {
                    printf("[WORLDGEN] No structure profile for biome %d\n", kind);
                }
            }
        }
    }
    // chunkgrid_mark_all(gChunks, map);
}
