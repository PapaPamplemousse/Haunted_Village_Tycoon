/**
 * @file music.h
 * @brief High-level music manager orchestrating background playback and events.
 */
#ifndef MUSIC_H
#define MUSIC_H

#include <stdbool.h>

/**
 * @brief Type of transition to apply when switching tracks.
 */
typedef enum MusicTransitionType
{
    MUSIC_TRANSITION_IMMEDIATE = 0, /**< Stop current music and start the new one instantly. */
    MUSIC_TRANSITION_FADE,          /**< Fade out current track before fading the new one in. */
    MUSIC_TRANSITION_CROSSFADE      /**< Blend both tracks together for the specified duration. */
} MusicTransitionType;

/**
 * @brief Initializes the music system and preloads tracks from configuration.
 *
 * @param configPath     Path to the .stv file with music metadata.
 * @param gameplayGroup  Optional group name to build the default gameplay playlist.
 * @return true on success, false on failure (audio remains disabled).
 */
bool music_system_init(const char* configPath, const char* gameplayGroup);

/**
 * @brief Releases all resources owned by the music system.
 */
void music_system_shutdown(void);

/**
 * @brief Updates streaming buffers, fades and automatic transitions.
 *
 * Call this once per frame from the main loop.
 *
 * @param deltaTime Frame delta time in seconds.
 */
void music_system_update(float deltaTime);

/**
 * @brief Sets the default lead time (in seconds) for gameplay crossfades.
 *
 * @param seconds Seconds before the end of the track where the next one should start.
 */
void music_system_set_gameplay_crossfade_lead(float seconds);

/**
 * @brief Rebuilds the gameplay playlist from the provided group.
 *
 * @param groupName          Group to use (NULL to include ungrouped tracks).
 * @param restartImmediately When true, restarts playback with the first track of the group.
 * @return true if at least one track belongs to the group, false otherwise.
 */
bool music_system_set_gameplay_group(const char* groupName, bool restartImmediately);

/**
 * @brief Forces playback to switch to the next track in the gameplay playlist.
 *
 * @param transition       Transition style to use.
 * @param durationSeconds  Fade duration for fade/crossfade transitions.
 */
void music_system_force_next(MusicTransitionType transition, float durationSeconds);

/**
 * @brief Triggers an event-specific track, overriding the gameplay playlist.
 *
 * @param eventName        Logical event key (case-insensitive).
 * @param transition       Transition style to reach the event track.
 * @param durationSeconds  Fade duration for fade/crossfade transitions.
 * @return true if the event track was found and scheduled, false otherwise.
 */
bool music_system_trigger_event(const char* eventName,
                                MusicTransitionType transition,
                                float                durationSeconds);

/**
 * @brief Cancels the current event override and returns to the gameplay playlist.
 *
 * @param transition       Transition style to re-enter the gameplay loop.
 * @param durationSeconds  Fade duration for fade/crossfade transitions.
 */
void music_system_clear_event(MusicTransitionType transition, float durationSeconds);

/**
 * @brief Sets a global multiplier applied to every track's volume.
 *
 * @param volume Value in range [0, 1].
 */
void music_system_set_master_volume(float volume);

/**
 * @brief Reports if an event override is currently active.
 *
 * @return true if an event track has control, false otherwise.
 */
bool music_system_is_event_active(void);

/**
 * @brief Returns how many gameplay groups are exposed (including the default "all" entry).
 */
int music_system_get_group_count(void);

/**
 * @brief Returns the name of the group at the given index (NULL represents the default group).
 */
const char* music_system_get_group_name(int index);

/**
 * @brief Returns the currently selected gameplay group index.
 */
int music_system_get_selected_group_index(void);

/**
 * @brief Convenience helper to select a gameplay group by index.
 */
bool music_system_set_gameplay_group_index(int index, bool restartImmediately);

/**
 * @brief Retrieves the display name of the currently playing track.
 */
const char* music_system_get_current_track_name(void);

/**
 * @brief Reports the current master volume multiplier.
 */
float music_system_get_master_volume(void);

#endif /* MUSIC_H */
