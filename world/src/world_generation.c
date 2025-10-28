/**
 * @file world_generation.c
 * @brief Generates terrain, climate maps, and structures for the world.
 */

#include "world_generation.h"
#include "tile.h"
#include "object.h"
#include "map.h"
#include "world.h"
#include "world_structures.h"
#include "biome_loader.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// If you want parallel generation, compile with -DWORLDGEN_USE_OPENMP -fopenmp
#if defined(WORLDGEN_USE_OPENMP)
#include <omp.h>
#endif

// ----------------------------------------------------------------------------------
// Small utils
// ----------------------------------------------------------------------------------
static inline int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline int in_bounds(int x, int y, int W, int H)
{
    return x >= 0 && y >= 0 && x < W && y < H;
}

// ----------------------------------------------------------------------------------
// Deterministic RNG (splitmix64)
// ----------------------------------------------------------------------------------
static uint64_t g_seed64 = 0x12345678ABCDEF01ull;

static uint64_t splitmix64_next(uint64_t* x)
{
    uint64_t z = (*x += 0x9E3779B97F4A7C15ull);
    z          = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z          = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
static float rng01(uint64_t* s)
{
    // 56-bit fraction → [0,1)
    return (splitmix64_next(s) >> 8) * (1.0f / (float)(1ull << 56));
}

// ----------------------------------------------------------------------------------
// Noises: value noise + fBm + light domain warping
// ----------------------------------------------------------------------------------
static float hash2i(int x, int y, uint32_t salt)
{
    // Fast integer hash, returns [0,1]
    uint32_t h = 2166136261u ^ (uint32_t)x;
    h          = (h ^ (uint32_t)y) * 16777619u;
    h ^= salt * 374761393u;
    return (h & 0x00FFFFFF) * (1.0f / 16777215.0f);
}
static float smooth(float t)
{
    return t * t * (3.0f - 2.0f * t);
}
static float lerpf(float a, float b, float t)
{
    return a + (b - a) * t;
}

static float value2D(float x, float y, uint32_t salt)
{
    int   xi = (int)floorf(x), yi = (int)floorf(y);
    float fx = x - xi, fy = y - yi;
    float v00 = hash2i(xi, yi, salt), v10 = hash2i(xi + 1, yi, salt);
    float v01 = hash2i(xi, yi + 1, salt), v11 = hash2i(xi + 1, yi + 1, salt);
    float u = smooth(fx), v = smooth(fy);
    return lerpf(lerpf(v00, v10, u), lerpf(v01, v11, u), v);
}
static float fbm2D(float x, float y, int octaves, float lac, float gain, float baseFreq, uint32_t salt)
{
    float amp = 1.0f, freq = baseFreq, sum = 0.0f, norm = 0.0f;
    for (int i = 0; i < octaves; i++)
    {
        sum += value2D(x * freq, y * freq, salt + (uint32_t)(i * 97u)) * amp;
        norm += amp;
        amp *= gain;
        freq *= lac;
    }
    return (norm > 0.0f) ? (sum / norm) : 0.0f;
}
static float warped2D(float x, float y, uint32_t salt)
{
    float dx = fbm2D(x + 13.7f, y - 9.1f, 3, 2.0f, 0.5f, 0.005f, 1337u ^ salt);
    float dy = fbm2D(x - 4.2f, y + 7.3f, 3, 2.0f, 0.5f, 0.005f, 7331u ^ salt);
    return fbm2D(x + (dx - 0.5f) * 150.0f, y + (dy - 0.5f) * 150.0f, 4, 2.0f, 0.5f, 0.0025f, 4242u ^ salt);
}

// ----------------------------------------------------------------------------------
// Parameters (kept compatible with your WorldGenParams)
// ----------------------------------------------------------------------------------
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
    .feature_density            = 0.08f,
    .structure_chance           = 0.0003f,
    .structure_min_spacing      = (MAP_WIDTH + MAP_HEIGHT) / 32,
    .biome_struct_mult_forest   = 0.4f,
    .biome_struct_mult_plain    = 1.0f,
    .biome_struct_mult_savanna  = 1.2f,
    .biome_struct_mult_tundra   = 0.5f,
    .biome_struct_mult_desert   = 0.3f,
    .biome_struct_mult_swamp    = 0.6f,
    .biome_struct_mult_mountain = 0.4f,
    .biome_struct_mult_cursed   = 0.8f,
    .biome_struct_mult_hell     = 0.2f,
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

// ----------------------------------------------------------------------------------
// Data-driven biome profiles (primary/secondary tiles + decor multipliers)
// ----------------------------------------------------------------------------------
typedef struct BiomeProfile
{
    BiomeKind  kind;
    TileTypeID primary;
    TileTypeID secondary;
    float      treeMul;
    float      bushMul;
    float      rockMul;
    float      structMul;
} BiomeProfile;

// ----------------------------------------------------------------------------------
// Climate maps (temperature, humidity, height) — coherent drivers
// ----------------------------------------------------------------------------------
typedef struct Climate
{
    float* temperature; // [H*W], normalized [0..1]
    float* humidity;    // [H*W], normalized [0..1]
    float* height;      // [H*W], normalized [0..1]
} Climate;

static void climate_build(Climate* c, int W, int H, uint64_t seed)
{
    (void)seed;
    c->temperature = (float*)malloc((size_t)W * H * sizeof(float));
    c->humidity    = (float*)malloc((size_t)W * H * sizeof(float));
    c->height      = (float*)malloc((size_t)W * H * sizeof(float));

    // Coherent fBm; temperature has a latitudinal gradient (colder north, warmer south)
#if defined(WORLDGEN_USE_OPENMP)
#pragma omp parallel for
#endif
    for (int y = 0; y < H; ++y)
    {
        float lat = (float)y / (float)H; // 0 north → 1 south
        for (int x = 0; x < W; ++x)
        {
            float nx = (float)x, ny = (float)y;
            float tempNoise = fbm2D(nx, ny, 4, 2.0f, 0.5f, 0.0028f, 1001u);
            float humidity  = fbm2D(nx, ny, 4, 2.0f, 0.5f, 0.0030f, 2003u);
            float height    = warped2D(nx * 0.7f, ny * 0.7f, 3001u);

            float tempLat = 0.5f + 0.4f * (0.5f - lat); // colder north
            float temp    = 0.5f * tempNoise + 0.5f * tempLat;

            c->temperature[y * W + x] = fminf(1.0f, fmaxf(0.0f, temp));
            c->humidity[y * W + x]    = fminf(1.0f, fmaxf(0.0f, 0.15f + 0.7f * humidity));
            c->height[y * W + x]      = fminf(1.0f, fmaxf(0.0f, height));
        }
    }
}

// Free allocated maps
static void climate_free(Climate* c)
{
    free(c->temperature);
    free(c->humidity);
    free(c->height);
    c->temperature = c->humidity = c->height = NULL;
}

// ----------------------------------------------------------------------------------
// Biome centers & macro Voronoi (fast assignment by macro-cells)
// ----------------------------------------------------------------------------------
static int nearest_center(const BiomeCenter* arr, int n, int x, int y)
{
    int best = -1, bestd = 0x7FFFFFFF;
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

// ----------------------------------------------------------------------------------
// Spawn biome centers with Poisson-like spacing, mandatory coverage, and unique biomes
// ----------------------------------------------------------------------------------
static int spawn_biome_centers(BiomeCenter* out, int maxN, int W, int H, int minDist, uint64_t* rs, const Climate* climate)
{
    int   n      = 0;
    float approx = (float)(W * H) / (3.14159f * minDist * minDist * 1.2f);
    int   target = (int)fmaxf(6.0f, fminf((float)maxN, approx));

    bool placedKind[BIO_MAX] = {0};

    // --- Required & unique biome rules ---
    const BiomeKind mustHave[]   = {BIO_FOREST, BIO_PLAIN, BIO_DESERT, BIO_TUNDRA, BIO_SAVANNA, BIO_SWAMP, BIO_MOUNTAIN};
    const BiomeKind uniqueOnes[] = {BIO_HELL, BIO_CURSED};
    const int       mustCount    = sizeof(mustHave) / sizeof(mustHave[0]);
    const int       uniqueCount  = sizeof(uniqueOnes) / sizeof(uniqueOnes[0]);

    // --- STEP 1: Place unique biomes once (e.g. Hell, Cursed) ---
    for (int i = 0; i < uniqueCount && n < maxN; i++)
    {
        BiomeKind       k  = uniqueOnes[i];
        const BiomeDef* bp = get_biome_def(k);

        // Adaptive radius for small maps
        int baseRadius = minDist;
        if (W < 256 || H < 256)
            baseRadius = (int)(fmaxf(W, H) / 3.0f);
        else
            baseRadius = (int)(minDist * 2.0f);

        bool  placed = false;
        int   bestX = -1, bestY = -1;
        float bestScore = -1.0f;

        for (int attempt = 0; attempt < 200; ++attempt)
        {
            int x = (int)(rng01(rs) * W);
            int y = (int)(rng01(rs) * H);

            float tempN  = climate->temperature[y * W + x];
            float humid  = climate->humidity[y * W + x];
            float height = climate->height[y * W + x];

            // Compute "score" for how well this spot fits the biome
            float score = 0.0f;
            if (k == BIO_HELL)
                score = tempN * (1.0f - humid) * (0.5f + height);
            else if (k == BIO_CURSED)
                score = (1.0f - tempN) * humid * (0.5f + height);

            if (score > bestScore)
            {
                bestScore = score;
                bestX     = x;
                bestY     = y;
            }

            // Direct accept if score is high enough
            if (score > 0.5f)
            {
                out[n++]      = (BiomeCenter){.x = x, .y = y, .kind = k, .primary = bp->primary, .secondary = bp->secondary};
                placedKind[k] = true;
                placed        = true;
                break;
            }
        }

        // If still not placed after all tries, use the best scoring spot found
        if (!placed && bestX >= 0)
        {
            out[n++]      = (BiomeCenter){.x = bestX, .y = bestY, .kind = k, .primary = bp->primary, .secondary = bp->secondary};
            placedKind[k] = true;
        }

        // Enlarge local distance to keep them visually distinct
        minDist = baseRadius;
    }

    // --- STEP 2: Ensure each common biome appears at least once ---
    for (int i = 0; i < mustCount && n < maxN; i++)
    {
        BiomeKind k = mustHave[i];
        if (placedKind[k])
            continue; // already placed (shouldn't happen)
        const BiomeDef* bp = get_biome_def(k);

        for (int attempt = 0; attempt < 30; ++attempt)
        {
            int x = (int)(rng01(rs) * W);
            int y = (int)(rng01(rs) * H);

            float tempN = climate->temperature[y * W + x];
            float humid = climate->humidity[y * W + x];

            bool ok = true;
            switch (k)
            {
                case BIO_DESERT:
                    ok = (tempN > 0.6f && humid < 0.3f);
                    break;
                case BIO_TUNDRA:
                    ok = (tempN < 0.4f);
                    break;
                case BIO_SWAMP:
                    ok = (humid > 0.7f);
                    break;
                default:
                    break;
            }

            if (!ok)
                continue;

            out[n++]      = (BiomeCenter){.x = x, .y = y, .kind = k, .primary = bp->primary, .secondary = bp->secondary};
            placedKind[k] = true;
            break;
        }
    }

    // --- STEP 3: Fill remaining centers with Poisson-style random spread (data-driven) ---
    for (int tries = 0; tries < target * 40 && n < target; ++tries)
    {
        int x = (int)(rng01(rs) * W);
        int y = (int)(rng01(rs) * H);

        // spacing check (Poisson-like)
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

        float tempN = climate->temperature[y * W + x];
        float humid = climate->humidity[y * W + x];
        float h     = climate->height[y * W + x];

        // --- Data-driven biome selection based on biomes.stv ---
        BiomeKind bestKind  = BIO_PLAIN;
        float     bestScore = -1.0f;

        for (int i = 0; i < gBiomeCount; ++i)
        {
            const BiomeDef* def = &gBiomeDefs[i];

            // Skip if maxInstances reached
            if (def->maxInstances > 0)
            {
                int count = 0;
                for (int j = 0; j < n; ++j)
                    if (out[j].kind == def->kind)
                        count++;
                if (count >= def->maxInstances)
                    continue;
            }

            // Check if climate fits biome ranges
            if (tempN >= def->tempMin && tempN <= def->tempMax && humid >= def->humidMin && humid <= def->humidMax && h >= def->heightMin && h <= def->heightMax)
            {
                // Score = closeness to the center of the range
                float tMid  = 0.5f * (def->tempMin + def->tempMax);
                float hMid  = 0.5f * (def->humidMin + def->humidMax);
                float zMid  = 0.5f * (def->heightMin + def->heightMax);
                float dist  = fabsf(tempN - tMid) + fabsf(humid - hMid) + fabsf(h - zMid);
                float score = 1.0f / (0.001f + dist);

                if (score > bestScore)
                {
                    bestScore = score;
                    bestKind  = def->kind;
                }
            }
        }

        if (bestScore < 0.0f)
            bestKind = BIO_PLAIN;

        const BiomeDef* bp = get_biome_def(bestKind);
        out[n++]           = (BiomeCenter){
                      .x         = x,
                      .y         = y,
                      .kind      = bestKind,
                      .primary   = bp ? bp->primary : TILE_GRASS,
                      .secondary = bp ? bp->secondary : TILE_GRASS,
        };
    }

    // --- STEP 4: Guarantee biome presence & enforce max limits ---
    for (int i = 0; i < gBiomeCount; ++i)
    {
        const BiomeDef* def = &gBiomeDefs[i];

        // Count occurrences
        int count = 0;
        for (int j = 0; j < n; ++j)
            if (out[j].kind == def->kind)
                count++;

        // Guarantee at least one instance
        if (count == 0)
        {
            int x = (int)(rng01(rs) * W);
            int y = (int)(rng01(rs) * H);

            out[n++] = (BiomeCenter){
                .x         = x,
                .y         = y,
                .kind      = def->kind,
                .primary   = def->primary,
                .secondary = def->secondary,
            };

            printf("⚠️  Added missing biome '%s' (forced spawn)\n", get_biome_name(def->kind));
        }

        // Enforce maxInstances if specified
        if (def->maxInstances > 0 && count > def->maxInstances)
        {
            int excess = count - def->maxInstances;
            for (int j = n - 1; j >= 0 && excess > 0; --j)
            {
                if (out[j].kind == def->kind)
                {
                    out[j].kind = BIO_PLAIN; // neutral replacement
                    excess--;
                }
            }
            printf("⚠️  Limited biome '%s' to %d instance(s)\n", get_biome_name(def->kind), def->maxInstances);
        }
    }

    return n;
}

// ----------------------------------------------------------------------------------
// Object placement helper
// ----------------------------------------------------------------------------------
static void maybe_place_object(Map* map, int x, int y, ObjectTypeID oid, float prob, uint64_t* rs)
{
    if (!in_bounds(x, y, map->width, map->height))
        return;
    if (map->objects[y][x] != NULL)
        return;
    if (rng01(rs) < prob)
        map_place_object(map, oid, x, y);
}

// ----------------------------------------------------------------------------------
// Terrain-aware lakes (water in basins, lava in hot/dry or hellish areas)
// ----------------------------------------------------------------------------------
static void generate_lakes(Map* map, const Climate* C, uint64_t* rng)
{
    const int W = map->width, H = map->height;

    // Frequency scaled by map size; fewer but larger coherent lakes
    int attempts = (W * H) / 6000;
    if (attempts < 2)
        attempts = 2;

    for (int i = 0; i < attempts; i++)
    {
        // Pick candidate around low height basins
        int cx = (int)(rng01(rng) * W);
        int cy = (int)(rng01(rng) * H);

        float h = C->height[cy * W + cx];
        float t = C->temperature[cy * W + cx];
        float u = C->humidity[cy * W + cx];

        // Bias selection toward basins (low height)
        if (h > 0.22f && rng01(rng) > 0.5f)
            continue;

        // Water or Lava decision
        int isLava = (t > 0.8f && u < 0.25f) || (map->tiles[cy][cx] == TILE_HELL) || (map->tiles[cy][cx] == TILE_LAVA);

        // Size driven by basin depth & humidity
        int rx = 3 + (int)(8 * (0.4f + (0.3f - h) * 1.2f));
        int ry = 2 + (int)(6 * (0.4f + u * 0.6f));
        rx     = clampi(rx, 3, 14);
        ry     = clampi(ry, 2, 10);

        TileTypeID fill = isLava ? TILE_LAVA : TILE_WATER;

        for (int y = cy - ry; y <= cy + ry; y++)
        {
            if (y < 0 || y >= H)
                continue;
            for (int x = cx - rx; x <= cx + rx; x++)
            {
                if (x < 0 || x >= W)
                    continue;
                float dx = (float)(x - cx) / (float)rx;
                float dy = (float)(y - cy) / (float)ry;
                if (dx * dx + dy * dy <= 1.0f)
                {
                    map->tiles[y][x]   = fill;
                    map->objects[y][x] = NULL;
                }
            }
        }
    }
}

typedef struct
{
    int           x;
    int           y;
    StructureKind kind;
} PlacedStructure;

static bool structure_allowed_in_biome(BiomeKind biome, StructureKind kind)
{
    const BiomeDef* def = get_biome_def(biome);
    if (!def || def->structureCount <= 0 || !def->structures)
        return false;

    for (int i = 0; i < def->structureCount; ++i)
    {
        if (def->structures[i].kind == kind)
            return true;
    }
    return false;
}

static bool respects_structure_spacing(int x, int y, const PlacedStructure* placed, int placedCount)
{
    if (g_cfg.structure_min_spacing <= 0)
        return true;

    int minSq = g_cfg.structure_min_spacing * g_cfg.structure_min_spacing;
    for (int i = 0; i < placedCount; ++i)
    {
        int dx = x - placed[i].x;
        int dy = y - placed[i].y;
        if (dx * dx + dy * dy < minSq)
            return false;
    }
    return true;
}

static bool attempt_spawn_structure(Map* map,
                                    const StructureDef* def,
                                    int x,
                                    int y,
                                    uint64_t* rng,
                                    PlacedStructure* placed,
                                    int* placedCount,
                                    int placedCap,
                                    int* structureCounts)
{
    if (!map || !def)
        return false;

    if (x < 1 || y < 1)
        return false;
    if (x + def->maxWidth + 1 >= map->width || y + def->maxHeight + 1 >= map->height)
        return false;

    if (!respects_structure_spacing(x, y, placed, *placedCount))
        return false;

    def->build(map, x, y, rng);

    if (structureCounts)
        structureCounts[def->kind]++;

    if (*placedCount < placedCap)
    {
        placed[*placedCount].x    = x;
        placed[*placedCount].y    = y;
        placed[*placedCount].kind = def->kind;
        (*placedCount)++;
    }

    return true;
}

// --- Local neighborhood 2-nearest centers with bi-frequency warp ---
// Search only the 3x3 macro-cell neighborhood to keep it fast.
static void pick_two_centers_from_neighbors(int* outBest1, int* outBest2, int x, int y, // tile coords
                                            int MC, int cellsX, int cellsY, const int* cellCenterIdx, const BiomeCenter* centers)
{
    const int cx = x / MC;
    const int cy = y / MC;

    // Bi-frequency domain warp (large soft bends + small capillaries)
    float wx = (float)x;
    float wy = (float)y;
    // large, low-frequency bend
    wx += (fbm2D(x * 0.006f, y * 0.006f, 2, 2.0f, 0.5f, 1.0f, 4242u) - 0.5f) * 36.0f;
    wy += (fbm2D((x + 913) * 0.006f, (y - 777) * 0.006f, 2, 2.0f, 0.5f, 1.0f, 5333u) - 0.5f) * 36.0f;
    // small, higher-frequency filaments
    wx += (fbm2D(x * 0.02f, y * 0.02f, 2, 2.0f, 0.5f, 1.0f, 9898u) - 0.5f) * 9.0f;
    wy += (fbm2D((x - 111) * 0.02f, (y + 222) * 0.02f, 2, 2.0f, 0.5f, 1.0f, 6767u) - 0.5f) * 9.0f;

    int   best1 = -1, best2 = -1;
    float d1 = 1e30f, d2 = 1e30f;

    // Scan 3x3 macro-cells around (cx,cy)
    for (int oy = -1; oy <= 1; ++oy)
    {
        int ncy = cy + oy;
        if (ncy < 0 || ncy >= cellsY)
            continue;
        for (int ox = -1; ox <= 1; ++ox)
        {
            int ncx = cx + ox;
            if (ncx < 0 || ncx >= cellsX)
                continue;

            int ci = cellCenterIdx[ncy * cellsX + ncx];
            if (ci < 0)
                continue;

            // Distance to candidate center (warped position)
            float dx   = wx - (float)centers[ci].x;
            float dy   = wy - (float)centers[ci].y;
            float dist = dx * dx + dy * dy;

            // Keep 2 best unique centers
            if (dist < d1 && ci != best1)
            {
                // shift down
                d2    = d1;
                best2 = best1;
                d1    = dist;
                best1 = ci;
            }
            else if (dist < d2 && ci != best1 && ci != best2)
            {
                d2    = dist;
                best2 = ci;
            }
        }
    }

    // Fallback: if neighborhood gave only one, duplicate (very rare)
    if (best1 < 0 && best2 >= 0)
    {
        best1 = best2;
        d1    = d2;
    }
    if (best1 >= 0 && best2 < 0)
    {
        best2 = best1;
        d2    = d1;
    }

    *outBest1 = best1;
    *outBest2 = best2;
}

// ----------------------------------------------------------------------------------
// Main generation
// ----------------------------------------------------------------------------------
void generate_world(Map* map)
{
    if (!map)
        return;
    const int W = map->width, H = map->height;

    load_structure_metadata("data/structures.stv");
    load_biome_definitions("data/biomes.stv");
    // 1) Build climate maps (coherent drivers)
    Climate C = {0};
    climate_build(&C, W, H, g_seed64);

    // 2) Spawn biome centers (Poisson-like) using climate & config
    const int   MAXC = 1024;
    BiomeCenter centers[MAXC];
    uint64_t    rs   = g_seed64;
    int         minR = g_cfg.min_biome_radius;
    int         nC   = spawn_biome_centers(centers, MAXC, W, H, minR, &rs, &C);
    printf("=== Spawned %d biome centers ===\n", nC);
    for (int i = 0; i < nC; i++)
    {
        const BiomeCenter* c = &centers[i];
        printf("[%3d] %s  pos=(%3d,%3d)\n", i, biome_kind_to_string(c->kind), c->x, c->y);
    }

    // 3) Macro-cell Voronoi assignment (fast). Each macro-cell selects its nearest center,
    //    then we fill tiles in the cell from that result. This avoids O(W*H*nC).
    const int MC     = 16; // macro-cell size in tiles; tweak 8..32 for quality vs speed
    const int cellsX = (W + MC - 1) / MC;
    const int cellsY = (H + MC - 1) / MC;

    // Precompute nearest center per macro-cell
    int* cellCenterIdx = (int*)malloc((size_t)cellsX * cellsY * sizeof(int));

// Parallelizable loop (optional OpenMP)
#if defined(WORLDGEN_USE_OPENMP)
#pragma omp parallel for collapse(2)
#endif
    for (int cy = 0; cy < cellsY; ++cy)
    {
        for (int cx = 0; cx < cellsX; ++cx)
        {
            int x = cx * MC + MC / 2;
            int y = cy * MC + MC / 2;
            if (x >= W)
                x = W - 1;
            if (y >= H)
                y = H - 1;

            float wx = x + (fbm2D(x, y, 2, 2.0f, 0.5f, 0.01f, 4242u) - 0.5f) * 40.0f;
            float wy = y + (fbm2D(x + 1000, y - 1000, 2, 2.0f, 0.5f, 0.01f, 4242u) - 0.5f) * 40.0f;

            int ci = nearest_center(centers, nC, (int)wx, (int)wy);

            // int ci                          = nearest_center(centers, nC, x, y);
            cellCenterIdx[cy * cellsX + cx] = ci;
        }
    }

    // 4) Paint tiles with soft biome blending and organic micro-variation
    const float warpFreq   = 0.004f; // cross-biome warping
    const float featherMin = 0.30f;  // inner blend edge
    const float featherMax = 0.70f;  // outer blend edge

#if defined(WORLDGEN_USE_OPENMP)
#pragma omp parallel for
#endif
    for (int y = 0; y < H; ++y)
    {
        const int cy = y / MC;
        for (int x = 0; x < W; ++x)
        {
            const int cx = x / MC;

            // Continents & extremes via height
            float h = C.height[y * W + x];
            if (h < 0.06f)
            {
                map->tiles[y][x]   = TILE_WATER;
                map->objects[y][x] = NULL;
                continue;
            }
            if (h > 0.97f)
            {
                map->tiles[y][x]   = TILE_LAVA;
                map->objects[y][x] = NULL;
                continue;
            }

            // --- Pick two nearest biome centers (A and B) ---
            int best1 = -1, best2 = -1;
            pick_two_centers_from_neighbors(&best1, &best2, x, y, MC, cellsX, cellsY, cellCenterIdx, centers);
            if (best1 < 0)
            {
                int ci = cellCenterIdx[cy * cellsX + cx];
                best1  = (ci >= 0) ? ci : 0;
                best2  = best1;
            }

            const BiomeCenter* A  = &centers[best1];
            const BiomeCenter* B  = &centers[best2];
            const BiomeDef*    pA = get_biome_def(A->kind);
            const BiomeDef*    pB = get_biome_def(B->kind);

            // --- Domain warping for soft “organic” borders ---
            float wx = (float)x + (fbm2D(x * warpFreq, y * warpFreq, 2, 2.0f, 0.5f, 1.0f, 999u) - 0.5f) * 80.0f;
            float wy = (float)y + (fbm2D((x + 913) * warpFreq, (y - 777) * warpFreq, 2, 2.0f, 0.5f, 1.0f, 888u) - 0.5f) * 80.0f;

            float dxA = wx - (float)A->x, dyA = wy - (float)A->y;
            float dxB = wx - (float)B->x, dyB = wy - (float)B->y;
            float dA = dxA * dxA + dyA * dyA;
            float dB = dxB * dxB + dyB * dyB;

            // --- Compute blend factor (0..1) with a smooth band ---
            float t     = dA / (dA + dB + 0.0001f);
            float blend = t * t * (3.0f - 2.0f * t); // smoothstep(0,1)
            blend       = fminf(1.0f, fmaxf(0.0f, (blend - featherMin) / (featherMax - featherMin)));

            // --- Frequency auto-scaling based on map size ---
            float worldScale = (float)(W + H) * 0.5f;

            // bigger maps → lower frequency (bigger features)
            float macroFreq = 1.5f / (worldScale * 0.001f);
            float microFreq = 0.08f / (worldScale * 0.001f);
            float warpFreq  = 3.0f / (worldScale * 0.001f);

            if (worldScale < 200)
            {
                macroFreq = 0.005f;
                microFreq = 1000.8f;
                warpFreq  = 500.0f;
            }
            float nx = (float)x / (float)W;
            float ny = (float)y / (float)H;

            // Large-scale patchiness: 0.5–1 patch per biome cell
            float macro = fbm2D(nx * macroFreq, ny * macroFreq, 4, 2.1f, 0.5f, 1.0f, 1337u ^ (uint32_t)A->kind) - 0.5f;

            // Fine detail: subtle texture within each patch
            float micro = fbm2D(x * microFreq, y * microFreq, 2, 2.0f, 0.5f, 1.0f, 4242u ^ (uint32_t)A->kind) - 0.5f;

            // Directional warp to make patches less circular
            float warpX = fbm2D(nx * warpFreq, ny * 1.0f, 2, 2.0f, 0.5f, 1.0f, 5555u) - 0.5f;
            float warpY = fbm2D(nx * 1.0f, ny * warpFreq, 2, 2.0f, 0.5f, 1.0f, 7777u) - 0.5f;

            // Blend macro + micro + warp
            float organic = macro * 0.7f + micro * 0.3f + (warpX + warpY) * 0.15f;

            // Biome-dependent bias
            float pSecondary = 0.3f + organic * 0.7f; // expand range into [0..1]

            // clamp
            if (pSecondary < 0.05f)
                pSecondary = 0.05f;
            if (pSecondary > 0.95f)
                pSecondary = 0.95f;

            // Use a stable hash per biome to decide final tile
            float rA = hash2i(x, y, 0xBEEFu ^ (uint32_t)A->kind);
            float rB = hash2i(x, y, 0xFEEDu ^ (uint32_t)B->kind);

            TileTypeID tileA = (rA < pSecondary) ? pA->secondary : pA->primary;
            TileTypeID tileB = (rB < pSecondary) ? pB->secondary : pB->primary;

            // --- Organic cross-biome blending ---
            float localNoise = fbm2D(x * 0.01f, y * 0.01f, 2, 2.0f, 0.5f, 1.0f, 444u);
            float mix        = blend + (localNoise - 0.5f) * 0.2f;
            if (mix < 0.0f)
                mix = 0.0f;
            else if (mix > 1.0f)
                mix = 1.0f;

            map->tiles[y][x]   = (mix < 0.5f) ? tileA : tileB;
            map->objects[y][x] = NULL;

#if 0 // optional debug sample output
        if (x % 50 == 0 && y % 50 == 0)
        {
            printf("[DEBUG TILE] biome=%s primary=%d secondary=%d pSec=%.2f chosen=%d mix=%.2f\n",
                   get_biome_name(A->kind), pA->primary, pA->secondary, pSecondary,
                   map->tiles[y][x], mix);
        }
#endif
        }
    }

// 5) Decor pass — probabilities modulated by climate & biome profile
//    (separate loop helps cache; also easier to tune)
#if defined(WORLDGEN_USE_OPENMP)
#pragma omp parallel for
#endif
    for (int y = 0; y < H; ++y)
    {
        for (int x = 0; x < W; ++x)
        {
            // Skip liquids/hazard hard-tiles
            TileTypeID t = map->tiles[y][x];
            if (t == TILE_WATER || t == TILE_LAVA)
                continue;

            int             ci = cellCenterIdx[(y / MC) * cellsX + (x / MC)];
            const BiomeDef* bp = get_biome_def(centers[ci].kind);

            // Climate influence
            float temp = C.temperature[y * W + x];
            float hum  = C.humidity[y * W + x];
            float h    = C.height[y * W + x];
            (void)temp;
            float fd = g_cfg.feature_density;

            // Trees prefer wet & lower alt (avoid deserts, peaks)
            float treeProb = fd * bp->treeMul * (0.2f + hum * 1.2f) * (h < 0.8f ? 1.0f : 0.3f);

            // Bushes prefer moderate moisture
            float bushProb = fd * bp->bushMul * (0.3f + 0.8f * hum) * 0.8f;

            // Rocks prefer dry/harsh terrain
            float rockProb = fd * bp->rockMul * (0.3f + (1.0f - h) * 0.7f) * (hum < 0.6f ? 1.0f : 0.5f);

            // Place per-biome props
            switch (centers[ci].kind)
            {
                case BIO_FOREST:
                case BIO_SWAMP:
                    maybe_place_object(map, x, y, OBJ_TREE, treeProb, &rs);
                    maybe_place_object(map, x, y, OBJ_STDBUSH, bushProb, &rs);
                    break;
                case BIO_PLAIN:
                    maybe_place_object(map, x, y, OBJ_STDBUSH, bushProb * 0.6f, &rs);
                    break;
                case BIO_SAVANNA:
                    maybe_place_object(map, x, y, OBJ_STDBUSH_DRY, bushProb * 1.1f, &rs);
                    maybe_place_object(map, x, y, OBJ_ROCK, rockProb * 0.6f, &rs);
                    break;
                case BIO_TUNDRA:
                    maybe_place_object(map, x, y, OBJ_DEAD_TREE, treeProb * 0.6f, &rs);
                    maybe_place_object(map, x, y, OBJ_ROCK, rockProb * 0.8f, &rs);
                    break;
                case BIO_DESERT:
                    maybe_place_object(map, x, y, OBJ_ROCK, rockProb * 1.2f, &rs);
                    break;
                case BIO_MOUNTAIN:
                    maybe_place_object(map, x, y, OBJ_ROCK, rockProb * 1.5f, &rs);
                    break;
                case BIO_CURSED:
                    maybe_place_object(map, x, y, OBJ_DEAD_TREE, treeProb * 1.0f, &rs);
                    maybe_place_object(map, x, y, OBJ_BONE_PILE, fd * 0.08f, &rs);
                    break;
                case BIO_HELL:
                    maybe_place_object(map, x, y, OBJ_SULFUR_VENT, fd * 0.05f, &rs);
                    break;
                case BIO_MAX:
                    break;
            }
        }
    }

    // 6) Lakes after base terrain to carve coherent patches (terrain-aware)
    generate_lakes(map, &C, &rs);

    // 7) Structures — data driven & scattered with spacing. We keep your API.
    //    Slightly reduced scan density by stepping over tiles (grid stride) to save time.
    //    Still deterministic, and spacing still enforced.
    PlacedStructure placed[1024];
    int             placedCount              = 0;
    int             structureCounts[STRUCT_COUNT] = {0};
    const int       placedCap                = (int)(sizeof(placed) / sizeof(placed[0]));

    const int STRIDE = 3; // check 1/STRIDE^2 of tiles as anchor candidates
    for (int y = 2; y < H - 10; y += STRIDE)
    {
        for (int x = 2; x < W - 10; x += STRIDE)
        {
            int       ci   = cellCenterIdx[(y / MC) * cellsX + (x / MC)];
            BiomeKind kind = centers[ci].kind;

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
                case BIO_MAX:
                    break;
            }

            float finalChance = g_cfg.structure_chance * biomeMult;
            if (rng01(&rs) < finalChance)
            {
                const StructureDef* def = pick_structure_for_biome(kind, &rs);
                if (def)
                {
                    attempt_spawn_structure(map, def, x, y, &rs, placed, &placedCount, placedCap, structureCounts);
                }
            }
        }
    }

    for (int k = 0; k < STRUCT_COUNT; ++k)
    {
        const StructureDef* def = get_structure_def((StructureKind)k);
        if (!def || def->minInstances <= 0)
            continue;

        int attempts    = 0;
        int maxAttempts = 1200;
        while (structureCounts[k] < def->minInstances && attempts < maxAttempts)
        {
            int maxX = W - def->maxWidth - 2;
            int maxY = H - def->maxHeight - 2;
            if (maxX <= 2 || maxY <= 2)
                break;

            int rangeX = maxX - 1;
            int rangeY = maxY - 1;
            if (rangeX <= 0 || rangeY <= 0)
                break;

            int x = 1 + (int)(rng01(&rs) * (float)rangeX);
            int y = 1 + (int)(rng01(&rs) * (float)rangeY);

            int cellX = x / MC;
            int cellY = y / MC;
            if (cellX < 0 || cellX >= cellsX || cellY < 0 || cellY >= cellsY)
            {
                attempts++;
                continue;
            }

            int centerIndex = cellCenterIdx[cellY * cellsX + cellX];
            if (centerIndex < 0 || centerIndex >= nC)
            {
                attempts++;
                continue;
            }

            BiomeKind biome = centers[centerIndex].kind;
            if (!structure_allowed_in_biome(biome, def->kind))
            {
                attempts++;
                continue;
            }

            if (attempt_spawn_structure(map, def, x, y, &rs, placed, &placedCount, placedCap, structureCounts))
            {
                attempts = 0;
                continue;
            }

            attempts++;
        }

        if (structureCounts[k] < def->minInstances)
        {
            printf("⚠️  Unable to satisfy minimum %d instances for structure %s (placed %d)\n",
                   def->minInstances,
                   def->name,
                   structureCounts[k]);
        }
    }

    // Cleanup
    free(cellCenterIdx);
    climate_free(&C);
}
