/**
 * @file world_time.h
 * @brief Time-of-day and season progression management.
 */

#ifndef WORLD_TIME_H
#define WORLD_TIME_H

#include <stdbool.h>

#include "world.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum SeasonKind
 * @brief Enumerates the repeating seasonal cycle used by the world simulation.
 */
typedef enum
{
    SEASON_SPRING = 0,
    SEASON_SUMMER,
    SEASON_AUTUMN,
    SEASON_WINTER
} SeasonKind;

/**
 * @struct WorldTime
 * @brief Aggregates state related to time progression and season management.
 */
typedef struct
{
    float      timeOfDay;       /**< Normalized [0,1) position within the current day (0=sunrise). */
    int        currentDay;      /**< Absolute day counter since the beginning of the simulation. */
    SeasonKind season;          /**< Currently active season. */
    float      secondsPerDay;   /**< Real-time duration of one in-game day (defaults to 600s). */
    int        timeWarpIndex;   /**< Index into the debug time warp presets (0 = real-time). */
    float      lastDeltaSeconds;/**< Actual simulated seconds advanced during the last update. */
} WorldTime;

void world_time_init(WorldTime* t);
void world_time_update(WorldTime* t, float deltaTime);
void world_time_cycle_timewarp(WorldTime* t);
float world_time_get_timewarp_multiplier(const WorldTime* t);
void world_time_draw_ui(const WorldTime* t, const Map* map, const Camera2D* camera);
void world_apply_season_effects(Map* map, const WorldTime* t);
float world_time_get_darkness(void);
int   world_time_get_current_day(void);
float world_time_get_time_of_day(void);
float world_time_get_seconds_per_day(void);
float world_time_get_last_step_seconds(void);

#ifdef __cplusplus
}
#endif

#endif /* WORLD_TIME_H */
