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
#include "road_planner.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

// If you want parallel generation, compile with -DWORLDGEN_USE_OPENMP -fopenmp
#if defined(WORLDGEN_USE_OPENMP)
#include <omp.h>
#endif

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
static float random01(uint64_t* rng)
{
    if (rng)
        return rng01(rng);
    return (float)rand() / ((float)RAND_MAX + 1.0f);
}

static int random_offset(uint64_t* rng, int radius)
{
    if (radius <= 0)
        return 0;
    if (rng)
    {
        uint64_t roll = splitmix64_next(rng);
        return (int)(roll % (uint64_t)(radius * 2 + 1)) - radius;
    }
    return (rand() % (radius * 2 + 1)) - radius;
}
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

        TileTypeID centerTile     = map->tiles[cy][cx];
        bool       centerHellish  = (centerTile == TILE_HELL) || (centerTile == TILE_LAVA);
        bool       centerSwampish = (centerTile == TILE_SWAMP) || (centerTile == TILE_CURSED_FOREST);
        bool       climateLava    = (t > 0.8f && u < 0.25f);

        // Size driven by basin depth & humidity
        int rx = 3 + (int)(8 * (0.4f + (0.3f - h) * 1.2f));
        int ry = 2 + (int)(6 * (0.4f + u * 0.6f));
        rx     = clampi(rx, 3, 14);
        ry     = clampi(ry, 2, 10);

        int totalSamples  = 0;
        int swampSamples  = 0;
        int cursedSamples = 0;
        int hellSamples   = 0;
        int lavaSamples   = 0;
        int waterSamples  = 0;
        int poisonSamples = 0;

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
                if (dx * dx + dy * dy > 1.0f)
                    continue;

                totalSamples++;
                TileTypeID sample = map->tiles[y][x];
                switch (sample)
                {
                    case TILE_SWAMP:
                        swampSamples++;
                        break;
                    case TILE_CURSED_FOREST:
                        cursedSamples++;
                        break;
                    case TILE_HELL:
                        hellSamples++;
                        break;
                    case TILE_LAVA:
                        lavaSamples++;
                        break;
                    case TILE_WATER:
                        waterSamples++;
                        break;
                    case TILE_POISON:
                        poisonSamples++;
                        break;
                    default:
                        break;
                }
            }
        }

        if (totalSamples == 0)
            continue;

        float swampCover   = (float)(swampSamples + cursedSamples) / (float)totalSamples;
        float hellCover    = (float)(hellSamples + lavaSamples) / (float)totalSamples;
        float liquidCover  = (float)(waterSamples + lavaSamples + poisonSamples) / (float)totalSamples;
        bool  areaLiquid   = liquidCover > 0.7f;
        bool  preferPoison = (swampCover > 0.55f) || (centerSwampish && swampCover > 0.3f);
        bool  preferLava   = centerHellish || climateLava || (hellCover > 0.35f);

        if (preferPoison && preferLava)
        {
            if (hellCover >= swampCover)
                preferPoison = false;
            else
                preferLava = false;
        }

        if (areaLiquid)
            continue;

        TileTypeID fill = TILE_WATER;
        if (preferLava)
            fill = TILE_LAVA;
        else if (preferPoison)
            fill = TILE_POISON;

        int maskWidth  = rx * 2 + 1;
        int maskHeight = ry * 2 + 1;
        int maskSize   = maskWidth * maskHeight;

        bool* mask = (bool*)calloc((size_t)maskSize, sizeof(bool));
        if (!mask)
            continue;

        float    orient     = rng01(rng) * 6.2831853f;
        float    cosA       = cosf(orient);
        float    sinA       = sinf(orient);
        float    axisScaleX = 0.65f + rng01(rng) * 1.15f;
        float    axisScaleY = 0.65f + rng01(rng) * 1.15f;
        float    offsetNX   = (rng01(rng) - 0.5f) * 0.6f;
        float    offsetNY   = (rng01(rng) - 0.5f) * 0.6f;
        float    lobeBias   = (rng01(rng) - 0.5f) * 0.9f;
        float    taperBias  = (rng01(rng) - 0.5f) * 0.35f;
        uint32_t coarseSalt = (uint32_t)splitmix64_next(rng);
        uint32_t detailSalt = (uint32_t)splitmix64_next(rng);

        int candidateCount = 0;

        for (int ly = -ry; ly <= ry; ++ly)
        {
            int gy = cy + ly;
            if (gy < 0 || gy >= H)
                continue;
            for (int lx = -rx; lx <= rx; ++lx)
            {
                int gx = cx + lx;
                if (gx < 0 || gx >= W)
                    continue;

                int localX = lx + rx;
                int localY = ly + ry;
                int idx    = localY * maskWidth + localX;

                float normX = (float)lx / (float)rx - offsetNX;
                float normY = (float)ly / (float)ry - offsetNY;

                float rotX = normX * cosA - normY * sinA;
                float rotY = normX * sinA + normY * cosA;

                rotX *= axisScaleX;
                rotY *= axisScaleY;

                float ellipse = rotX * rotX + rotY * rotY;
                if (ellipse > 2.6f)
                    continue;

                float radial = sqrtf(ellipse);

                float coarse  = fbm2D((float)gx * 0.05f, (float)gy * 0.05f, 3, 2.0f, 0.5f, 1.0f, 811u ^ coarseSalt) - 0.5f;
                float detail  = fbm2D((float)gx * 0.16f, (float)gy * 0.16f, 2, 2.0f, 0.5f, 1.0f, 1223u ^ detailSalt) - 0.5f;
                float angular = fbm2D(rotX * 2.8f + 17.0f, rotY * 2.8f - 11.0f, 2, 2.0f, 0.5f, 1.0f, 1999u ^ (coarseSalt >> 1)) - 0.5f;

                float threshold = 1.05f;
                threshold += coarse * (0.9f + radial * 0.6f);
                threshold += detail * 0.45f;
                threshold += angular * 0.35f;
                threshold += lobeBias * rotX;
                threshold += taperBias * rotY;
                threshold -= radial * 0.25f;

                if (threshold < 0.4f)
                    threshold = 0.4f;
                if (threshold > 1.95f)
                    threshold = 1.95f;

                if (ellipse <= threshold)
                {
                    mask[idx] = true;
                    candidateCount++;
                }
            }
        }

        int minOrganicArea = (int)(totalSamples * 0.35f);
        if (minOrganicArea < 6)
            minOrganicArea = 6;

        bool fallbackEllipse = (candidateCount < minOrganicArea);

        if (!fallbackEllipse)
        {
            for (int ly = 0; ly < maskHeight; ++ly)
            {
                int gy = cy + ly - ry;
                if (gy < 0 || gy >= H)
                    continue;
                for (int lx = 0; lx < maskWidth; ++lx)
                {
                    if (!mask[ly * maskWidth + lx])
                        continue;

                    int gx = cx + lx - rx;
                    if (gx < 0 || gx >= W)
                        continue;

                    map->tiles[gy][gx]   = fill;
                    map->objects[gy][gx] = NULL;
                }
            }
        }

        free(mask);

        if (fallbackEllipse)
        {
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
}

typedef struct
{
    int           x;
    int           y;
    StructureKind kind;
    int           doorX;
    int           doorY;
    int           boundsX;
    int           boundsY;
    int           boundsW;
    int           boundsH;
} PlacedStructure;

static bool structure_allowed_in_biome(BiomeKind biome, StructureKind kind);
static bool attempt_spawn_structure(Map* map, const StructureDef* def, int anchorX, int anchorY, uint64_t* rng, PlacedStructure* placed, int* placedCount, int placedCap, int* structureCounts, bool fromCluster);
static bool place_cluster_member_instance(Map* map, const StructureDef* def, float anchorCenterX, float anchorCenterY, float halfWidth, float halfHeight, uint64_t* rng, PlacedStructure* placed, int* placedCount,
                                          int placedCap, int* structureCounts);
static bool is_floor_tile(TileTypeID id);
static bool find_structure_door(const Map* map, int startX, int startY, int width, int height, int* outX, int* outY);
static bool compute_door_exit(const Map* map, int doorX, int doorY, int* outX, int* outY);
static void paint_road_tile(Map* map, int x, int y);
static bool rectangles_overlap_margin(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh, int margin);
static bool bounds_overlap_existing(int startX, int startY, int width, int height, const PlacedStructure* placed, int placedCount, int margin);
static bool tile_walkable_for_road(const Map* map, int x, int y, int startX, int startY, int goalX, int goalY, const PlacedStructure* placed, int placedCount);
static const PlacedStructure* structure_at_point(const PlacedStructure* placed, int placedCount, int x, int y);
static void                   apply_road_step(Map* map, int x, int y, const PlacedStructure* placed, int placedCount);
static bool                   find_road_path(Map* map, int startX, int startY, int goalX, int goalY, const PlacedStructure* placed, int placedCount, RoadPoint** outPath, int* outCount);
static void                   carve_road_between(Map* map, int x0, int y0, int x1, int y1, const PlacedStructure* placed, int placedCount);
static bool                   is_cannibal_structure(StructureKind kind);
static void                   connect_cannibal_structures(Map* map, const PlacedStructure* placed, int placedCount);

static void spawn_cluster_members(Map* map, const StructureDef* anchor, int baseX, int baseY, uint64_t* rng, PlacedStructure* placed, int* placedCount, int placedCap, int* structureCounts)
{
    if (!map || !anchor || !anchor->clusterAnchor || anchor->clusterMemberCount <= 0)
        return;

    float widthRef  = (anchor->maxWidth > 0) ? (float)anchor->maxWidth : (float)anchor->minWidth;
    float heightRef = (anchor->maxHeight > 0) ? (float)anchor->maxHeight : (float)anchor->minHeight;
    float centerX   = (float)baseX + widthRef * 0.5f;
    float centerY   = (float)baseY + heightRef * 0.5f;

    float radiusMin = (anchor->clusterRadiusMin > 0.0f) ? anchor->clusterRadiusMin : (widthRef + heightRef) * 0.35f;
    float radiusMax = (anchor->clusterRadiusMax > radiusMin) ? anchor->clusterRadiusMax : radiusMin + 3.0f;

    float baseSize = fmaxf(widthRef, heightRef);
    if (baseSize <= 0.0f)
        baseSize = 6.0f;
    float preferredMin = baseSize * 1.1f;
    float preferredMax = preferredMin * 2.2f;

    if (radiusMin <= 0.0f)
        radiusMin = preferredMin;
    else
        radiusMin = fminf(radiusMin, preferredMin);

    if (radiusMax <= radiusMin)
        radiusMax = radiusMin + preferredMin;
    else
        radiusMax = fminf(radiusMax, preferredMax);

    int desiredMin = anchor->clusterMinMembers > 0 ? anchor->clusterMinMembers : 0;
    int desiredMax = anchor->clusterMaxMembers > 0 ? anchor->clusterMaxMembers : INT_MAX;

    int                 plannedCounts[STRUCTURE_CLUSTER_MAX_MEMBERS] = {0};
    int                 maxCounts[STRUCTURE_CLUSTER_MAX_MEMBERS]     = {0};
    const StructureDef* memberDefs[STRUCTURE_CLUSTER_MAX_MEMBERS]    = {0};

    int totalPlanned = 0;
    for (int m = 0; m < anchor->clusterMemberCount; ++m)
    {
        const StructureClusterMember* member = &anchor->clusterMembers[m];
        if (member->kind <= STRUCT_HUT_CANNIBAL || member->kind >= STRUCT_COUNT)
            continue;

        const StructureDef* memberDef = get_structure_def(member->kind);
        if (!memberDef)
            continue;

        int minCount = member->minCount < 0 ? 0 : member->minCount;
        int maxCount = member->maxCount < minCount ? minCount : member->maxCount;

        if (structureCounts && memberDef->maxInstances > 0)
        {
            int remaining = memberDef->maxInstances - structureCounts[memberDef->kind];
            if (remaining <= 0)
            {
                memberDefs[m]    = memberDef;
                maxCounts[m]     = 0;
                plannedCounts[m] = 0;
                continue;
            }
            if (maxCount > remaining)
                maxCount = remaining;
            if (minCount > remaining)
                minCount = remaining;
        }

        if (desiredMax != INT_MAX)
        {
            int remaining = desiredMax - totalPlanned;
            if (remaining <= 0)
            {
                memberDefs[m]    = memberDef;
                maxCounts[m]     = maxCount;
                plannedCounts[m] = 0;
                continue;
            }
            if (maxCount > remaining)
                maxCount = remaining;
            if (minCount > remaining)
                minCount = remaining;
        }

        if (maxCount <= 0 && minCount <= 0)
        {
            memberDefs[m]    = memberDef;
            maxCounts[m]     = maxCount;
            plannedCounts[m] = 0;
            continue;
        }

        int toSpawn = minCount;
        if (maxCount > minCount)
        {
            if (rng)
            {
                uint64_t roll = splitmix64_next(rng);
                toSpawn += (int)(roll % (uint64_t)(maxCount - minCount + 1));
            }
            else
            {
                toSpawn += rand() % (maxCount - minCount + 1);
            }
        }

        plannedCounts[m] = toSpawn;
        maxCounts[m]     = maxCount;
        memberDefs[m]    = memberDef;
        totalPlanned += toSpawn;
    }

    if (totalPlanned < desiredMin)
    {
        bool progress = true;
        while (totalPlanned < desiredMin && progress)
        {
            progress = false;
            for (int m = 0; m < anchor->clusterMemberCount && totalPlanned < desiredMin; ++m)
            {
                const StructureDef* memberDef = memberDefs[m];
                if (!memberDef)
                    continue;

                if (maxCounts[m] == 0)
                    continue;
                if (maxCounts[m] > 0 && plannedCounts[m] >= maxCounts[m])
                    continue;

                if (structureCounts && memberDef->maxInstances > 0)
                {
                    int remaining = memberDef->maxInstances - structureCounts[memberDef->kind] - plannedCounts[m];
                    if (remaining <= 0)
                        continue;
                }

                plannedCounts[m]++;
                totalPlanned++;
                progress = true;
            }
        }
    }

    int   totalStructures = 1;
    float sumWidths       = widthRef > 0.0f ? widthRef : 4.0f;
    float sumHeights      = heightRef > 0.0f ? heightRef : 4.0f;

    for (int m = 0; m < anchor->clusterMemberCount; ++m)
    {
        const StructureDef* memberDef = memberDefs[m];
        int                 count     = plannedCounts[m];
        if (!memberDef || count <= 0)
            continue;

        float memberWidth  = (memberDef->maxWidth > 0) ? (float)memberDef->maxWidth : (float)memberDef->minWidth;
        float memberHeight = (memberDef->maxHeight > 0) ? (float)memberDef->maxHeight : (float)memberDef->minHeight;
        if (memberWidth <= 0.0f)
            memberWidth = widthRef > 0.0f ? widthRef : 4.0f;
        if (memberHeight <= 0.0f)
            memberHeight = heightRef > 0.0f ? heightRef : 4.0f;

        sumWidths += memberWidth * (float)count;
        sumHeights += memberHeight * (float)count;
        totalStructures += count;
    }

    if (totalStructures <= 0)
        totalStructures = 1;

    float avgWidth  = sumWidths / (float)totalStructures;
    float avgHeight = sumHeights / (float)totalStructures;
    if (avgWidth <= 0.0f)
        avgWidth = widthRef > 0.0f ? widthRef : 6.0f;
    if (avgHeight <= 0.0f)
        avgHeight = heightRef > 0.0f ? heightRef : 6.0f;

    float baseSpacingF = fmaxf(avgWidth, avgHeight) + 2.0f;
    int   spacing      = (int)roundf(baseSpacingF);
    if (spacing < 4)
        spacing = 4;

    typedef struct
    {
        int  x;
        int  y;
        bool used;
    } ClusterCandidate;

    ClusterCandidate candidates[128];
    int              candidateCount = 0;
    int              maxRing        = (int)ceilf((radiusMax > 0.0f ? radiusMax : (float)spacing * 3.0f) / (float)spacing);
    if (maxRing < 1)
        maxRing = 1;

    int centerXi = (int)roundf(centerX);
    int centerYi = (int)roundf(centerY);

    for (int ring = 1; ring <= maxRing && candidateCount < (int)(sizeof(candidates) / sizeof(candidates[0])); ++ring)
    {
        for (int dy = -ring; dy <= ring && candidateCount < (int)(sizeof(candidates) / sizeof(candidates[0])); ++dy)
        {
            for (int dx = -ring; dx <= ring && candidateCount < (int)(sizeof(candidates) / sizeof(candidates[0])); ++dx)
            {
                if (abs(dx) != ring && abs(dy) != ring)
                    continue;
                int px                       = centerXi + dx * spacing;
                int py                       = centerYi + dy * spacing;
                candidates[candidateCount++] = (ClusterCandidate){px, py, false};
            }
        }
    }

    if (candidateCount == 0)
    {
        candidates[candidateCount++] = (ClusterCandidate){centerXi + spacing, centerYi, false};
        candidates[candidateCount++] = (ClusterCandidate){centerXi - spacing, centerYi, false};
        candidates[candidateCount++] = (ClusterCandidate){centerXi, centerYi + spacing, false};
        candidates[candidateCount++] = (ClusterCandidate){centerXi, centerYi - spacing, false};
    }

    if (rng && candidateCount > 1)
    {
        for (int i = candidateCount - 1; i > 0; --i)
        {
            uint64_t         roll = splitmix64_next(rng);
            int              j    = (int)(roll % (uint64_t)(i + 1));
            ClusterCandidate tmp  = candidates[i];
            candidates[i]         = candidates[j];
            candidates[j]         = tmp;
        }
    }

    typedef struct
    {
        const StructureDef* def;
        int                 memberIndex;
        float               priority;
    } SpawnRequest;

    SpawnRequest requests[STRUCTURE_CLUSTER_MAX_MEMBERS * 8];
    int          requestCount = 0;

    for (int m = 0; m < anchor->clusterMemberCount; ++m)
    {
        const StructureDef* memberDef = memberDefs[m];
        int                 toSpawn   = plannedCounts[m];
        if (!memberDef || toSpawn <= 0)
            continue;

        float memberWidth  = (memberDef->maxWidth > 0) ? (float)memberDef->maxWidth : (float)memberDef->minWidth;
        float memberHeight = (memberDef->maxHeight > 0) ? (float)memberDef->maxHeight : (float)memberDef->minHeight;
        if (memberWidth <= 0.0f)
            memberWidth = widthRef > 0.0f ? widthRef : 4.0f;
        if (memberHeight <= 0.0f)
            memberHeight = heightRef > 0.0f ? heightRef : 4.0f;

        float priority = memberWidth * memberHeight;

        for (int count = 0; count < toSpawn && requestCount < (int)(sizeof(requests) / sizeof(requests[0])); ++count)
            requests[requestCount++] = (SpawnRequest){memberDef, m, priority};
    }

    for (int i = 1; i < requestCount; ++i)
    {
        SpawnRequest key = requests[i];
        int          j   = i - 1;
        while (j >= 0 && requests[j].priority < key.priority)
        {
            requests[j + 1] = requests[j];
            --j;
        }
        requests[j + 1] = key;
    }

    int   totalSpawned                                    = 0;
    int   spawnedPerMember[STRUCTURE_CLUSTER_MAX_MEMBERS] = {0};
    int   candidateCursor                                 = 0;
    int   candidateLimit                                  = candidateCount;
    float fallbackHalf                                    = (float)spacing * 1.6f;

    for (int r = 0; r < requestCount; ++r)
    {
        if (desiredMax != INT_MAX && totalSpawned >= desiredMax)
            break;

        const StructureDef* memberDef = requests[r].def;
        int                 m         = requests[r].memberIndex;

        bool placedMember = false;
        if (candidateLimit > 0)
        {
            for (int attempt = 0; attempt < candidateLimit; ++attempt)
            {
                int idx = (candidateCursor + attempt) % candidateLimit;
                if (candidates[idx].used)
                    continue;

                int cx     = candidates[idx].x;
                int cy     = candidates[idx].y;
                int jitter = spacing / 3;
                if (jitter > 0)
                {
                    cx += random_offset(rng, jitter);
                    cy += random_offset(rng, jitter);
                }

                if (attempt_spawn_structure(map, memberDef, cx, cy, rng, placed, placedCount, placedCap, structureCounts, true))
                {
                    candidates[idx].used = true;
                    candidateCursor      = (idx + 1) % candidateLimit;
                    spawnedPerMember[m]++;
                    totalSpawned++;
                    placedMember = true;
                    break;
                }
            }
        }

        if (!placedMember)
        {
            if (place_cluster_member_instance(map, memberDef, centerX, centerY, fallbackHalf, fallbackHalf, rng, placed, placedCount, placedCap, structureCounts))
            {
                spawnedPerMember[m]++;
                totalSpawned++;
            }
        }
    }

    if (totalSpawned < desiredMin)
    {
        for (int m = 0; m < anchor->clusterMemberCount && totalSpawned < desiredMin; ++m)
        {
            const StructureDef* memberDef = memberDefs[m];
            if (!memberDef)
                continue;

            while (totalSpawned < desiredMin)
            {
                if (maxCounts[m] > 0 && spawnedPerMember[m] >= maxCounts[m])
                    break;
                if (desiredMax != INT_MAX && totalSpawned >= desiredMax)
                    break;
                if (structureCounts && memberDef->maxInstances > 0 && structureCounts[memberDef->kind] + spawnedPerMember[m] >= memberDef->maxInstances)
                    break;

                bool placedMember = false;
                if (candidateLimit > 0)
                {
                    for (int idx = 0; idx < candidateLimit; ++idx)
                    {
                        if (candidates[idx].used)
                            continue;
                        int cx = candidates[idx].x + random_offset(rng, spacing / 3);
                        int cy = candidates[idx].y + random_offset(rng, spacing / 3);
                        if (attempt_spawn_structure(map, memberDef, cx, cy, rng, placed, placedCount, placedCap, structureCounts, true))
                        {
                            candidates[idx].used = true;
                            spawnedPerMember[m]++;
                            totalSpawned++;
                            placedMember = true;
                            break;
                        }
                    }
                }

                if (!placedMember)
                {
                    if (!place_cluster_member_instance(map, memberDef, centerX, centerY, fallbackHalf, fallbackHalf, rng, placed, placedCount, placedCap, structureCounts))
                        break;
                    spawnedPerMember[m]++;
                    totalSpawned++;
                }
            }
        }
    }
}

static bool structure_spacing_ok(float centerX, float centerY, const PlacedStructure* placed, int placedCount, float minSpacing)
{
    if (!placed || placedCount <= 0 || minSpacing <= 0.0f)
        return true;

    const float minSpacingSq = minSpacing * minSpacing;
    for (int i = 0; i < placedCount; ++i)
    {
        float dx = centerX - (float)placed[i].x;
        float dy = centerY - (float)placed[i].y;
        if ((dx * dx + dy * dy) < minSpacingSq)
            return false;
    }

    return true;
}

static bool structure_area_clear(Map* map, int startX, int startY, int width, int height)
{
    if (!map)
        return false;

    const int W = map->width;
    const int H = map->height;

    for (int y = startY - 1; y <= startY + height; ++y)
    {
        if (y < 0 || y >= H)
            return false;
        for (int x = startX - 1; x <= startX + width; ++x)
        {
            if (x < 0 || x >= W)
                return false;

            TileTypeID tile = map->tiles[y][x];
            TileType*  type = get_tile_type(tile);
            if (!type)
                return false;

            if (type->category == TILE_CATEGORY_WATER || type->category == TILE_CATEGORY_HAZARD || type->category == TILE_CATEGORY_OBSTACLE || !type->walkable)
            {
                return false;
            }
        }
    }

    return true;
}

static void structure_clear_objects(Map* map, int startX, int startY, int width, int height)
{
    if (!map)
        return;

    const int W = map->width;
    const int H = map->height;

    for (int y = startY - 1; y <= startY + height; ++y)
    {
        if (y < 0 || y >= H)
            continue;
        for (int x = startX - 1; x <= startX + width; ++x)
        {
            if (x < 0 || x >= W)
                continue;
            if (map->objects[y][x])
                map_remove_object(map, x, y);
        }
    }
}

static bool rectangles_overlap_margin(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh, int margin)
{
    int aLeft   = ax - margin;
    int aRight  = ax + aw - 1 + margin;
    int aTop    = ay - margin;
    int aBottom = ay + ah - 1 + margin;

    int bLeft   = bx - margin;
    int bRight  = bx + bw - 1 + margin;
    int bTop    = by - margin;
    int bBottom = by + bh - 1 + margin;

    if (aRight < bLeft || bRight < aLeft)
        return false;
    if (aBottom < bTop || bBottom < aTop)
        return false;
    return true;
}

static bool bounds_overlap_existing(int startX, int startY, int width, int height, const PlacedStructure* placed, int placedCount, int margin)
{
    if (!placed || placedCount <= 0)
        return false;

    for (int i = 0; i < placedCount; ++i)
    {
        const PlacedStructure* other = &placed[i];
        if (other->boundsW <= 0 || other->boundsH <= 0)
            continue;
        if (rectangles_overlap_margin(startX, startY, width, height, other->boundsX, other->boundsY, other->boundsW, other->boundsH, margin))
            return true;
    }

    return false;
}

static const PlacedStructure* structure_at_point(const PlacedStructure* placed, int placedCount, int x, int y)
{
    if (!placed || placedCount <= 0)
        return NULL;

    for (int i = 0; i < placedCount; ++i)
    {
        const PlacedStructure* ps = &placed[i];
        if (ps->boundsW <= 0 || ps->boundsH <= 0)
            continue;
        if (x >= ps->boundsX && x < ps->boundsX + ps->boundsW && y >= ps->boundsY && y < ps->boundsY + ps->boundsH)
            return ps;
    }

    return NULL;
}

static bool is_floor_tile(TileTypeID id)
{
    return id == TILE_WOOD_FLOOR || id == TILE_STRAW_FLOOR || id == TILE_STONE_FLOOR;
}

static bool find_structure_door(const Map* map, int startX, int startY, int width, int height, int* outX, int* outY)
{
    if (!map || !outX || !outY)
        return false;

    int  foundX = -1;
    int  foundY = -1;
    bool found  = false;

    int endX = startX + width;
    int endY = startY + height;
    for (int y = startY - 1; y <= endY; ++y)
    {
        for (int x = startX - 1; x <= endX; ++x)
        {
            if (!in_bounds(x, y, map->width, map->height))
                continue;
            Object* obj = map->objects[y][x];
            if (obj && obj->type && obj->type->isDoor)
            {
                foundX = x;
                foundY = y;
                found  = true;
                goto door_found;
            }
        }
    }

door_found:
    if (found)
    {
        *outX = foundX;
        *outY = foundY;
        return true;
    }

    *outX = -1;
    *outY = -1;
    return false;
}

static bool compute_door_exit(const Map* map, int doorX, int doorY, int* outX, int* outY)
{
    if (!map)
        return false;

    static const int OFFSETS[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int i = 0; i < 4; ++i)
    {
        int nx = doorX + OFFSETS[i][0];
        int ny = doorY + OFFSETS[i][1];
        if (!in_bounds(nx, ny, map->width, map->height))
            continue;

        TileTypeID neighbor = map->tiles[ny][nx];
        if (is_floor_tile(neighbor))
        {
            int exitX = doorX - OFFSETS[i][0];
            int exitY = doorY - OFFSETS[i][1];
            if (!in_bounds(exitX, exitY, map->width, map->height))
            {
                exitX = doorX;
                exitY = doorY;
            }
            if (outX)
                *outX = exitX;
            if (outY)
                *outY = exitY;
            return true;
        }
    }

    if (outX)
        *outX = doorX;
    if (outY)
        *outY = doorY;
    return false;
}

static void paint_road_tile(Map* map, int x, int y)
{
    if (!map)
        return;
    if (!in_bounds(x, y, map->width, map->height))
        return;
    map_set_tile(map, x, y, TILE_MUD_ROAD);
}

static bool tile_walkable_for_road(const Map* map, int x, int y, int startX, int startY, int goalX, int goalY, const PlacedStructure* placed, int placedCount)
{
    if (!map)
        return false;
    if (!in_bounds(x, y, map->width, map->height))
        return false;

    if ((x == startX && y == startY) || (x == goalX && y == goalY))
        return true;

    const PlacedStructure* occupant = structure_at_point(placed, placedCount, x, y);
    if (occupant)
    {
        if (!(x == occupant->doorX && y == occupant->doorY))
            return false;
    }

    TileTypeID tid  = map->tiles[y][x];
    TileType*  type = get_tile_type(tid);
    if (!type)
        return false;

    if (!type->walkable)
        return false;
    if (type->category == TILE_CATEGORY_WATER || type->category == TILE_CATEGORY_HAZARD || type->category == TILE_CATEGORY_OBSTACLE)
        return false;

    return true;
}

static void apply_road_step(Map* map, int x, int y, const PlacedStructure* placed, int placedCount)
{
    if (!map)
        return;
    if (!in_bounds(x, y, map->width, map->height))
        return;

    const PlacedStructure* occupant = structure_at_point(placed, placedCount, x, y);
    if (occupant && !(x == occupant->doorX && y == occupant->doorY))
        return;

    Object* obj = map->objects[y][x];
    if (obj && obj->type)
    {
        if (obj->type->isWall)
            return;
        if (!obj->type->isDoor)
            map_remove_object(map, x, y);
    }

    paint_road_tile(map, x, y);
}

static bool find_road_path(Map* map, int startX, int startY, int goalX, int goalY, const PlacedStructure* placed, int placedCount, RoadPoint** outPath, int* outCount)
{
    if (!map || !outPath || !outCount)
        return false;

    int margin = 12;
    int minX   = clampi((startX < goalX ? startX : goalX) - margin, 0, map->width - 1);
    int maxX   = clampi((startX > goalX ? startX : goalX) + margin, 0, map->width - 1);
    int minY   = clampi((startY < goalY ? startY : goalY) - margin, 0, map->height - 1);
    int maxY   = clampi((startY > goalY ? startY : goalY) + margin, 0, map->height - 1);

    if (minX > maxX || minY > maxY)
        return false;

    int width  = maxX - minX + 1;
    int height = maxY - minY + 1;
    int total  = width * height;
    if (total <= 0 || total > 32768)
        return false;

    int* queue = (int*)malloc(sizeof(int) * total);
    int* prev  = (int*)malloc(sizeof(int) * total);
    if (!queue || !prev)
    {
        free(queue);
        free(prev);
        return false;
    }

    for (int i = 0; i < total; ++i)
        prev[i] = -1;

    int startIdx = (startY - minY) * width + (startX - minX);
    int goalIdx  = (goalY - minY) * width + (goalX - minX);

    int head       = 0;
    int tail       = 0;
    queue[tail++]  = startIdx;
    prev[startIdx] = startIdx;

    static const int DIRS[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    bool found = false;
    while (head < tail)
    {
        int idx = queue[head++];
        if (idx == goalIdx)
        {
            found = true;
            break;
        }

        int cx = idx % width + minX;
        int cy = idx / width + minY;

        for (int d = 0; d < 4; ++d)
        {
            int nx = cx + DIRS[d][0];
            int ny = cy + DIRS[d][1];
            if (nx < minX || nx > maxX || ny < minY || ny > maxY)
                continue;

            int nIdx = (ny - minY) * width + (nx - minX);
            if (prev[nIdx] != -1)
                continue;

            if (!tile_walkable_for_road(map, nx, ny, startX, startY, goalX, goalY, placed, placedCount))
                continue;

            prev[nIdx]    = idx;
            queue[tail++] = nIdx;
        }
    }

    if (!found)
    {
        free(queue);
        free(prev);
        return false;
    }

    int count = 0;
    for (int idx = goalIdx;; idx = prev[idx])
    {
        ++count;
        if (idx == startIdx)
            break;
    }

    RoadPoint* path = (RoadPoint*)malloc(sizeof(RoadPoint) * count);
    if (!path)
    {
        free(queue);
        free(prev);
        return false;
    }

    int idx = goalIdx;
    for (int i = count - 1; i >= 0; --i)
    {
        int px    = idx % width + minX;
        int py    = idx / width + minY;
        path[i].x = px;
        path[i].y = py;
        if (idx == startIdx)
            break;
        idx = prev[idx];
    }

    free(queue);
    free(prev);

    *outPath  = path;
    *outCount = count;
    return true;
}

static bool point_blocked_by_structure(int x, int y, const PlacedStructure* placed, int placedCount)
{
    const PlacedStructure* ps = structure_at_point(placed, placedCount, x, y);
    if (!ps)
        return false;
    return !(x == ps->doorX && y == ps->doorY);
}

static void carve_road_between(Map* map, int x0, int y0, int x1, int y1, const PlacedStructure* placed, int placedCount)
{
    if (!map)
        return;

    RoadPoint* path      = NULL;
    int        pathCount = 0;

    if (find_road_path(map, x0, y0, x1, y1, placed, placedCount, &path, &pathCount))
    {
        for (int i = 0; i < pathCount; ++i)
            apply_road_step(map, path[i].x, path[i].y, placed, placedCount);
        free(path);
        return;
    }

    // Fallback: simple Manhattan trace that respects building bounds.
    int x = x0;
    int y = y0;
    apply_road_step(map, x, y, placed, placedCount);

    while (x != x1)
    {
        int stepX = (x1 > x) ? 1 : -1;
        int nx    = x + stepX;
        if (point_blocked_by_structure(nx, y, placed, placedCount))
            break;
        x = nx;
        apply_road_step(map, x, y, placed, placedCount);
    }

    while (y != y1)
    {
        int stepY = (y1 > y) ? 1 : -1;
        int ny    = y + stepY;
        if (point_blocked_by_structure(x, ny, placed, placedCount))
            break;
        y = ny;
        apply_road_step(map, x, y, placed, placedCount);
    }

    if (x == x1 && y == y1)
        apply_road_step(map, x1, y1, placed, placedCount);
}

static bool is_cannibal_structure(StructureKind kind)
{
    switch (kind)
    {
        case STRUCT_HUT_CANNIBAL:
        case STRUCT_CANNIBAL_LONGHOUSE:
        case STRUCT_CANNIBAL_COOK_TENT:
        case STRUCT_CANNIBAL_SHAMAN_HUT:
        case STRUCT_CANNIBAL_BONE_PIT:
            return true;
        default:
            return false;
    }
}

static void connect_cannibal_structures(Map* map, const PlacedStructure* placed, int placedCount)
{
    if (!map || !placed || placedCount <= 1)
        return;

    enum
    {
        MAX_CANNIBAL_ROAD_NODES = 32
    };
    typedef struct
    {
        RoadPoint     door;
        RoadPoint     anchor;
        StructureKind kind;
    } CannibalRoadNode;

    CannibalRoadNode nodes[MAX_CANNIBAL_ROAD_NODES];
    int              nodeCount = 0;

    for (int i = 0; i < placedCount && nodeCount < MAX_CANNIBAL_ROAD_NODES; ++i)
    {
        const PlacedStructure* ps = &placed[i];
        if (!is_cannibal_structure(ps->kind))
            continue;
        if (ps->doorX < 0 || ps->doorY < 0)
            continue;

        int exitX = ps->doorX;
        int exitY = ps->doorY;
        compute_door_exit(map, ps->doorX, ps->doorY, &exitX, &exitY);

        nodes[nodeCount].door.x   = ps->doorX;
        nodes[nodeCount].door.y   = ps->doorY;
        nodes[nodeCount].anchor.x = exitX;
        nodes[nodeCount].anchor.y = exitY;
        nodes[nodeCount].kind     = ps->kind;
        nodeCount++;
    }

    if (nodeCount <= 1)
        return;

    int anchorIndex = 0;
    for (int i = 0; i < nodeCount; ++i)
    {
        if (nodes[i].kind == STRUCT_CANNIBAL_LONGHOUSE)
        {
            anchorIndex = i;
            break;
        }
    }
    if (anchorIndex != 0)
    {
        CannibalRoadNode tmp = nodes[0];
        nodes[0]             = nodes[anchorIndex];
        nodes[anchorIndex]   = tmp;
    }

    RoadPoint points[MAX_CANNIBAL_ROAD_NODES];
    for (int i = 0; i < nodeCount; ++i)
        points[i] = nodes[i].anchor;

    int order[MAX_CANNIBAL_ROAD_NODES];
    int result = tsp_plan_route(points, nodeCount, order, MAX_CANNIBAL_ROAD_NODES);
    if (result <= 0)
        return;

    CannibalRoadNode ordered[MAX_CANNIBAL_ROAD_NODES];
    for (int i = 0; i < nodeCount; ++i)
        ordered[i] = nodes[order[i]];

    for (int i = 0; i < nodeCount; ++i)
    {
        apply_road_step(map, ordered[i].door.x, ordered[i].door.y, placed, placedCount);
        if (ordered[i].door.x != ordered[i].anchor.x || ordered[i].door.y != ordered[i].anchor.y)
            carve_road_between(map, ordered[i].door.x, ordered[i].door.y, ordered[i].anchor.x, ordered[i].anchor.y, placed, placedCount);
    }

    for (int i = 0; i < nodeCount - 1; ++i)
        carve_road_between(map, ordered[i].anchor.x, ordered[i].anchor.y, ordered[i + 1].anchor.x, ordered[i + 1].anchor.y, placed, placedCount);
}

static bool place_cluster_member_instance(Map* map, const StructureDef* def, float anchorCenterX, float anchorCenterY, float halfWidth, float halfHeight, uint64_t* rng, PlacedStructure* placed, int* placedCount,
                                          int placedCap, int* structureCounts)
{
    if (!map || !def || !def->build)
        return false;

    if (structureCounts && def->maxInstances > 0 && structureCounts[def->kind] >= def->maxInstances)
        return false;

    if (halfWidth < 1.0f)
        halfWidth = 1.0f;
    if (halfHeight < 1.0f)
        halfHeight = 1.0f;

    const int tries = 16;
    for (int attempt = 0; attempt < tries; ++attempt)
    {
        float offsetX        = (random01(rng) * 2.0f - 1.0f) * halfWidth;
        float offsetY        = (random01(rng) * 2.0f - 1.0f) * halfHeight;
        float candidateCX    = anchorCenterX + offsetX;
        float candidateCY    = anchorCenterY + offsetY;
        int   roundedCenterX = (int)roundf(candidateCX);
        int   roundedCenterY = (int)roundf(candidateCY);

        if (attempt_spawn_structure(map, def, roundedCenterX, roundedCenterY, rng, placed, placedCount, placedCap, structureCounts, true))
        {
            return true;
        }
    }

    return false;
}

static bool attempt_spawn_structure(Map* map, const StructureDef* def, int anchorX, int anchorY, uint64_t* rng, PlacedStructure* placed, int* placedCount, int placedCap, int* structureCounts, bool fromCluster)
{
    if (!map || !def || !def->build)
        return false;

    if (structureCounts && def->maxInstances > 0 && structureCounts[def->kind] >= def->maxInstances)
        return false;

    int widthMax  = (def->maxWidth > 0) ? def->maxWidth : def->minWidth;
    int heightMax = (def->maxHeight > 0) ? def->maxHeight : def->minHeight;
    if (widthMax <= 0 || heightMax <= 0)
        return false;

    const int minX = 1;
    const int minY = 1;
    const int maxX = map->width - widthMax - 1;
    const int maxY = map->height - heightMax - 1;

    if (maxX < minX || maxY < minY)
        return false;

    float baseSpacing = (float)g_cfg.structure_min_spacing;
    if (baseSpacing <= 0.0f)
        baseSpacing = (float)(widthMax + heightMax);

    float spacing = fromCluster ? fmaxf(2.0f, baseSpacing * 0.35f) : baseSpacing;

    const int jitter   = fromCluster ? 2 : 4;
    const int attempts = fromCluster ? 12 : 24;

    for (int attempt = 0; attempt < attempts; ++attempt)
    {
        int candidateCX = anchorX + random_offset(rng, jitter);
        int candidateCY = anchorY + random_offset(rng, jitter);

        int startX = candidateCX - widthMax / 2;
        int startY = candidateCY - heightMax / 2;

        startX = clampi(startX, minX, maxX);
        startY = clampi(startY, minY, maxY);

        float centerXf = (float)startX + (float)widthMax * 0.5f;
        float centerYf = (float)startY + (float)heightMax * 0.5f;

        if (!structure_spacing_ok(centerXf, centerYf, placed, placedCount ? *placedCount : 0, spacing))
            continue;

        if (bounds_overlap_existing(startX, startY, widthMax, heightMax, placed, placedCount ? *placedCount : 0, 1))
            continue;

        if (!structure_area_clear(map, startX, startY, widthMax, heightMax))
            continue;

        structure_clear_objects(map, startX, startY, widthMax, heightMax);

        def->build(map, startX, startY, rng);

        int doorX = -1;
        int doorY = -1;
        find_structure_door(map, startX, startY, widthMax, heightMax, &doorX, &doorY);

        if (structureCounts)
            structureCounts[def->kind]++;

        if (placed && placedCount && *placedCount < placedCap)
        {
            placed[*placedCount].x       = (int)roundf(centerXf);
            placed[*placedCount].y       = (int)roundf(centerYf);
            placed[*placedCount].kind    = def->kind;
            placed[*placedCount].doorX   = doorX;
            placed[*placedCount].doorY   = doorY;
            placed[*placedCount].boundsX = startX;
            placed[*placedCount].boundsY = startY;
            placed[*placedCount].boundsW = widthMax;
            placed[*placedCount].boundsH = heightMax;
            (*placedCount)++;
        }

        if (def->clusterAnchor && !fromCluster)
        {
            spawn_cluster_members(map, def, startX, startY, rng, placed, placedCount, placedCap, structureCounts);
        }

        return true;
    }

    return false;
}

static bool structure_allowed_in_biome(BiomeKind biome, StructureKind kind)
{
    if (biome < 0 || biome >= BIO_MAX)
        return false;

    const StructureDef* def = get_structure_def(kind);
    if (!def)
        return false;

    if (def->allowedBiomesMask == 0)
        return true;

    uint32_t mask = 1u << biome;
    return (def->allowedBiomesMask & mask) != 0;
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
            if (t == TILE_WATER || t == TILE_LAVA || t == TILE_POISON)
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
    int             placedCount                   = 0;
    int             structureCounts[STRUCT_COUNT] = {0};
    const int       placedCap                     = (int)(sizeof(placed) / sizeof(placed[0]));

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
                const StructureDef* def = pick_structure_for_biome(kind, &rs, structureCounts);
                if (def)
                {
                    if (def->maxInstances > 0 && structureCounts[def->kind] >= def->maxInstances)
                        continue;
                    attempt_spawn_structure(map, def, x, y, &rs, placed, &placedCount, placedCap, structureCounts, false);
                }
            }
        }
    }

    for (int k = 0; k < STRUCT_COUNT; ++k)
    {
        const StructureDef* def = get_structure_def((StructureKind)k);
        if (!def || def->minInstances <= 0)
            continue;

        int required = def->minInstances;
        if (def->maxInstances > 0 && required > def->maxInstances)
            required = def->maxInstances;
        if (required <= 0)
            continue;

        int attempts    = 0;
        int maxAttempts = 1200;
        while (structureCounts[k] < required && attempts < maxAttempts)
        {
            if (def->maxInstances > 0 && structureCounts[k] >= def->maxInstances)
                break;
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

            if (attempt_spawn_structure(map, def, x, y, &rs, placed, &placedCount, placedCap, structureCounts, false))
            {
                attempts = 0;
                continue;
            }

            attempts++;
        }

        if (structureCounts[k] < required)
        {
            printf("⚠️  Unable to satisfy minimum %d instances for structure %s (placed %d)\n", required, def->name, structureCounts[k]);
        }
    }

    connect_cannibal_structures(map, placed, placedCount);

    // Cleanup
    free(cellCenterIdx);
    climate_free(&C);
}
