/**
 * @file world_time.c
 * @brief Implements the global time-of-day and seasonal simulation.
 */

#include "world_time.h"

#include <math.h>
#include <stdio.h>

#include "raylib.h"

#include "biome_loader.h"
#include "tile.h"
#include "ui_theme.h"

static const float s_timeWarpMultipliers[] = {1.0f, 6.0f, 24.0f, 72.0f};
static const int   s_timeWarpCount         = (int)(sizeof(s_timeWarpMultipliers) / sizeof(s_timeWarpMultipliers[0]));

static float      s_baseFertility[TILE_MAX];
static float      s_baseHumidity[TILE_MAX];
static float      s_baseTemperature[TILE_MAX];
static bool       s_baselineCaptured = false;
static int        s_tileCounts[TILE_MAX];
static int        s_totalTiles       = 0;
static bool       s_countsReady      = false;
static float      s_avgFertility     = 0.0f;
static float      s_avgHumidity      = 0.0f;
static float      s_avgTemperature   = 0.0f;
static float      s_currentDarkness  = 0.0f;
static int        s_biomeTileCounts[BIO_MAX];
static float      s_biomeAvgFertility[BIO_MAX];
static float      s_biomeAvgHumidity[BIO_MAX];
static float      s_biomeAvgTemperature[BIO_MAX];

static const char* season_to_string(SeasonKind season)
{
    switch (season)
    {
        case SEASON_SPRING:
            return "SPRING";
        case SEASON_SUMMER:
            return "SUMMER";
        case SEASON_AUTUMN:
            return "AUTUMN";
        case SEASON_WINTER:
            return "WINTER";
        default:
            return "UNKNOWN";
    }
}

static SeasonKind season_next(SeasonKind s)
{
    return (SeasonKind)(((int)s + 1) % 4);
}

static BiomeKind biome_from_tile(TileTypeID id)
{
    switch (id)
    {
        case TILE_FOREST:
            return BIO_FOREST;
        case TILE_GRASS:
        case TILE_PLAIN:
            return BIO_PLAIN;
        case TILE_SAVANNA:
            return BIO_SAVANNA;
        case TILE_TUNDRA:
        case TILE_TUNDRA_2:
            return BIO_TUNDRA;
        case TILE_DESERT:
            return BIO_DESERT;
        case TILE_SWAMP:
            return BIO_SWAMP;
        case TILE_MOUNTAIN:
            return BIO_MOUNTAIN;
        case TILE_CURSED_FOREST:
        case TILE_POISON:
            return BIO_CURSED;
        case TILE_HELL:
        case TILE_LAVA:
            return BIO_HELL;
        case TILE_WATER:
            return BIO_PLAIN;
        default:
            return BIO_PLAIN;
    }
}

static float season_daylight_fraction(SeasonKind season)
{
    switch (season)
    {
        case SEASON_SPRING:
            return 0.55f;
        case SEASON_SUMMER:
            return 0.65f;
        case SEASON_AUTUMN:
            return 0.50f;
        case SEASON_WINTER:
        default:
            return 0.35f;
    }
}

static float smoothstep(float edge0, float edge1, float x)
{
    if (x <= edge0)
        return 0.0f;
    if (x >= edge1)
        return 1.0f;
    float t = (x - edge0) / (edge1 - edge0);
    return t * t * (3.0f - 2.0f * t);
}

static float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

static float compute_darkness(const WorldTime* t)
{
    float dayFraction   = season_daylight_fraction(t->season);
    float transition    = fminf(0.12f, dayFraction * 0.3f);
    float sunriseEnd    = fminf(transition, dayFraction);
    float sunriseStart  = 1.0f - transition;
    float sunsetStart   = dayFraction - transition;
    if (sunsetStart < sunriseEnd)
        sunsetStart = sunriseEnd;
    float sunsetEnd = dayFraction + transition;
    if (sunsetEnd > 1.0f)
        sunsetEnd = 1.0f;

    float tod = t->timeOfDay;
    if (tod < 0.0f)
        tod += 1.0f;
    else if (tod >= 1.0f)
        tod -= 1.0f;

    const float dawnResidualDarkness = 0.35f;

    if (tod >= sunriseStart)
    {
        float blend = smoothstep(sunriseStart, 1.0f, tod);
        return lerp(1.0f, dawnResidualDarkness, blend);
    }

    if (tod < sunriseEnd)
    {
        float blend = smoothstep(0.0f, sunriseEnd, tod);
        return lerp(dawnResidualDarkness, 0.0f, blend);
    }

    if (tod < sunsetStart)
        return 0.0f;
    if (tod < sunsetEnd)
        return smoothstep(sunsetStart, sunsetEnd, tod);
    return 1.0f;
}

static void capture_baseline(void)
{
    if (s_baselineCaptured)
        return;

    for (int i = 0; i < TILE_MAX; ++i)
    {
        s_baseFertility[i]   = tileTypes[i].fertility;
        s_baseHumidity[i]    = tileTypes[i].humidity;
        s_baseTemperature[i] = tileTypes[i].temperature;
        tileTypes[i].darkness = 0.0f;
    }
    s_baselineCaptured = true;
}

static void ensure_tile_counts(Map* map)
{
    if (s_countsReady || !map)
        return;

    s_totalTiles = 0;
    for (int i = 0; i < TILE_MAX; ++i)
        s_tileCounts[i] = 0;
    for (int i = 0; i < BIO_MAX; ++i)
        s_biomeTileCounts[i] = 0;

    for (int y = 0; y < map->height; ++y)
        for (int x = 0; x < map->width; ++x)
        {
            TileTypeID id = map->tiles[y][x];
            if (id >= 0 && id < TILE_MAX)
            {
                s_tileCounts[id]++;
                s_totalTiles++;
                BiomeKind biome = biome_from_tile(id);
                if (biome >= 0 && biome < BIO_MAX)
                    s_biomeTileCounts[biome]++;
            }
        }

    s_countsReady = (s_totalTiles > 0);
}

static void update_averages(void)
{
    if (!s_countsReady || s_totalTiles <= 0)
        return;

    double sumF = 0.0;
    double sumH = 0.0;
    double sumT = 0.0;
    double biomeSumF[BIO_MAX] = {0.0};
    double biomeSumH[BIO_MAX] = {0.0};
    double biomeSumT[BIO_MAX] = {0.0};

    for (int i = 0; i < TILE_MAX; ++i)
    {
        if (s_tileCounts[i] == 0)
            continue;

        double weightedF = (double)tileTypes[i].fertility * (double)s_tileCounts[i];
        double weightedH = (double)tileTypes[i].humidity * (double)s_tileCounts[i];
        double weightedT = (double)tileTypes[i].temperature * (double)s_tileCounts[i];

        sumF += weightedF;
        sumH += weightedH;
        sumT += weightedT;

        BiomeKind biome = biome_from_tile((TileTypeID)i);
        if (biome >= 0 && biome < BIO_MAX)
        {
            biomeSumF[biome] += weightedF;
            biomeSumH[biome] += weightedH;
            biomeSumT[biome] += weightedT;
        }
    }

    s_avgFertility   = (float)(sumF / s_totalTiles);
    s_avgHumidity    = (float)(sumH / s_totalTiles);
    s_avgTemperature = (float)(sumT / s_totalTiles);

    for (int i = 0; i < BIO_MAX; ++i)
    {
        if (s_biomeTileCounts[i] > 0)
        {
            s_biomeAvgFertility[i]   = (float)(biomeSumF[i] / (double)s_biomeTileCounts[i]);
            s_biomeAvgHumidity[i]    = (float)(biomeSumH[i] / (double)s_biomeTileCounts[i]);
            s_biomeAvgTemperature[i] = (float)(biomeSumT[i] / (double)s_biomeTileCounts[i]);
        }
        else
        {
            s_biomeAvgFertility[i]   = 0.0f;
            s_biomeAvgHumidity[i]    = 0.0f;
            s_biomeAvgTemperature[i] = 0.0f;
        }
    }
}

void world_time_init(WorldTime* t)
{
    if (!t)
        return;

    capture_baseline();

    t->secondsPerDay   = 600.0f;
    t->timeOfDay       = 0.0f;
    t->currentDay      = 1;
    t->season          = SEASON_SPRING;
    t->timeWarpIndex   = 0;
    t->lastDeltaSeconds = 0.0f;

    s_currentDarkness = 0.0f;
    s_countsReady     = false;
    s_avgFertility    = 0.0f;
    s_avgHumidity     = 0.0f;
    s_avgTemperature  = 0.0f;
    for (int i = 0; i < BIO_MAX; ++i)
    {
        s_biomeTileCounts[i]    = 0;
        s_biomeAvgFertility[i]  = 0.0f;
        s_biomeAvgHumidity[i]   = 0.0f;
        s_biomeAvgTemperature[i]= 0.0f;
    }
}

void world_time_cycle_timewarp(WorldTime* t)
{
    if (!t)
        return;

    t->timeWarpIndex = (t->timeWarpIndex + 1) % s_timeWarpCount;
}

float world_time_get_timewarp_multiplier(const WorldTime* t)
{
    if (!t)
        return 1.0f;

    int idx = t->timeWarpIndex;
    if (idx < 0)
        idx = 0;
    if (idx >= s_timeWarpCount)
        idx %= s_timeWarpCount;
    return s_timeWarpMultipliers[idx];
}

void world_time_update(WorldTime* t, float deltaTime)
{
    if (!t)
        return;

    float timeScale = world_time_get_timewarp_multiplier(t);
    float scaledDelta  = deltaTime * timeScale;
    t->lastDeltaSeconds = scaledDelta;

    if (t->secondsPerDay <= 0.0f)
        t->secondsPerDay = 600.0f;

    t->timeOfDay += scaledDelta / t->secondsPerDay;

    while (t->timeOfDay >= 1.0f)
    {
        t->timeOfDay -= 1.0f;
        t->currentDay++;
        if (t->currentDay > 1 && ((t->currentDay - 1) % 10) == 0)
            t->season = season_next(t->season);
    }

    if (t->timeOfDay < 0.0f)
        t->timeOfDay += 1.0f;

    s_currentDarkness = compute_darkness(t);

    for (int i = 0; i < TILE_MAX; ++i)
        tileTypes[i].darkness = s_currentDarkness;
}

void world_apply_season_effects(Map* map, const WorldTime* t)
{
    if (!t)
        return;

    ensure_tile_counts(map);

    typedef struct
    {
        float fertilityOffset;
        float humidityOffset;
        float temperatureOffset;
    } SeasonModifiers;

    static const SeasonModifiers modifiers[] = {
        [SEASON_SPRING] = {+0.12f, +0.10f, 0.0f},
        [SEASON_SUMMER] = {+0.02f, -0.15f, +8.0f},
        [SEASON_AUTUMN] = {-0.08f, -0.05f, -2.0f},
        [SEASON_WINTER] = {-0.15f, -0.18f, -12.0f},
    };

    SeasonModifiers active = modifiers[t->season];

    float dt = (t->lastDeltaSeconds > 0.0f) ? t->lastDeltaSeconds : 0.016f;
    float blend = fminf(1.0f, dt * 0.2f + 0.02f);

    for (int i = 0; i < TILE_MAX; ++i)
    {
        float targetF = s_baseFertility[i] + active.fertilityOffset;
        float targetH = s_baseHumidity[i] + active.humidityOffset;
        float targetT = s_baseTemperature[i] + active.temperatureOffset;

        if (targetF < 0.0f)
            targetF = 0.0f;
        if (targetF > 1.0f)
            targetF = 1.0f;

        if (targetH < 0.0f)
            targetH = 0.0f;
        if (targetH > 1.0f)
            targetH = 1.0f;

        tileTypes[i].fertility += (targetF - tileTypes[i].fertility) * blend;
        tileTypes[i].humidity += (targetH - tileTypes[i].humidity) * blend;
        tileTypes[i].temperature += (targetT - tileTypes[i].temperature) * blend;
    }

    update_averages();
}

float world_time_get_darkness(void)
{
    return s_currentDarkness;
}

void world_time_draw_ui(const WorldTime* t, const Map* map, const Camera2D* camera)
{
    if (!t)
        return;

    char infoLine[128];
    float hours   = t->timeOfDay * 24.0f;
    int   hour    = (int)hours;
    int   minute  = (int)((hours - (float)hour) * 60.0f);
    if (hour < 0)
        hour = 0;
    if (hour > 23)
        hour = 23;
    if (minute < 0)
        minute = 0;
    if (minute > 59)
        minute = 59;

    snprintf(infoLine, sizeof(infoLine), "Day: %02d | Season: %s | Time: %02d:%02d",
             t->currentDay,
             season_to_string(t->season),
             hour,
             minute);

    float warp = world_time_get_timewarp_multiplier(t);
    char  warpLine[160];
    if (warp > 1.0f)
        snprintf(warpLine, sizeof(warpLine), "Accélération x%.0f | Obscurité %.2f", warp, s_currentDarkness);
    else
        snprintf(warpLine, sizeof(warpLine), "Obscurité %.2f | T pour accélérer", s_currentDarkness);

    const char* biomeName       = "GLOBAL";
    float       biomeFertility  = s_avgFertility;
    float       biomeHumidity   = s_avgHumidity;
    float       biomeTemp       = s_avgTemperature;
    int         biomeTiles      = s_totalTiles;
    bool        biomeStatsValid = false;

    if (map && camera)
    {
        float focusX = camera->target.x;
        float focusY = camera->target.y;
        int   tileX  = (int)floorf(focusX / (float)TILE_SIZE);
        int   tileY  = (int)floorf(focusY / (float)TILE_SIZE);

        if (tileX >= 0 && tileX < map->width && tileY >= 0 && tileY < map->height)
        {
            TileTypeID tid   = map->tiles[tileY][tileX];
            BiomeKind  biome = biome_from_tile(tid);
            if (biome >= 0 && biome < BIO_MAX && s_biomeTileCounts[biome] > 0)
            {
                biomeStatsValid = true;
                biomeName       = get_biome_name(biome);
                if (!biomeName)
                    biomeName = "UNKNOWN";
                biomeTiles     = s_biomeTileCounts[biome];
                biomeFertility = s_biomeAvgFertility[biome];
                biomeHumidity  = s_biomeAvgHumidity[biome];
                biomeTemp      = s_biomeAvgTemperature[biome];
            }
        }
    }

    if (!biomeStatsValid && biomeTiles <= 0)
        biomeTiles = s_totalTiles;

    char statsLine[200];
    snprintf(statsLine, sizeof(statsLine), "Biome %s (%d) | Fert %.2f | Humid %.2f | %.1fC",
             biomeName,
             biomeTiles,
             biomeFertility,
             biomeHumidity,
             biomeTemp);
    const UiTheme* ui = ui_theme_get();
    Color textPrimary   = ui ? ui->textPrimary : WHITE;
    Color textSecondary = ui ? ui->textSecondary : ColorAlpha(WHITE, 0.85f);
    Color textAccent    = ui ? ui->accent : (Color){255, 200, 120, 255};

    const int mainFont   = 22;
    const int secondaryFont = 18;
    float padding = 16.0f;

    float width = (float)MeasureText(infoLine, mainFont);
    float warpWidth = (float)MeasureText(warpLine, secondaryFont);
    float statsWidth = (float)MeasureText(statsLine, secondaryFont);
    width = fmaxf(width, fmaxf(warpWidth, statsWidth));

    Rectangle panel = {20.0f, 20.0f, width + padding * 2.0f, mainFont + secondaryFont * 2.0f + padding * 3.0f};

    if (ui && ui_theme_is_ready())
        DrawTextureNPatch(ui->atlas, ui->panelSmall, panel, (Vector2){0.0f, 0.0f}, 0.0f, ColorAlpha(WHITE, 0.95f));
    else
        DrawRectangleRec(panel, ColorAlpha(BLACK, 0.5f));

    float textX = panel.x + padding;
    float textY = panel.y + padding;
    DrawText(infoLine, (int)textX, (int)textY, mainFont, textPrimary);

    textY += mainFont + 6.0f;
    Color warpColor = (warp > 1.0f) ? textAccent : textSecondary;
    DrawText(warpLine, (int)textX, (int)textY, secondaryFont, warpColor);

    textY += secondaryFont + 6.0f;
    DrawText(statsLine, (int)textX, (int)textY, secondaryFont, textSecondary);
}
