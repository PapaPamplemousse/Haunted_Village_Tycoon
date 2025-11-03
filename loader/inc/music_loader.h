/**
 * @file music_loader.h
 * @brief Declares utilities that deserialize music metadata (.stv format).
 */
#ifndef MUSIC_LOADER_H
#define MUSIC_LOADER_H

#include <stdbool.h>

/**
 * @brief Describes the intended usage of a music track.
 */
typedef enum MusicUsage
{
    MUSIC_USAGE_UNKNOWN = 0, /**< Track has no explicit usage tag. */
    MUSIC_USAGE_GAMEPLAY,    /**< Regular gameplay loop music. */
    MUSIC_USAGE_EVENT,       /**< Music tied to specific in-game events. */
    MUSIC_USAGE_AMBIENT,     /**< Ambient or background layers. */
    MUSIC_USAGE_MENU         /**< Menu or UI background music. */
} MusicUsage;

/**
 * @brief Metadata loaded from an .stv music definition entry.
 */
typedef struct MusicDefinition
{
    int         id;                /**< Numeric identifier for the track (optional). */
    char*       name;              /**< Display name for debugging or UI. */
    char*       filePath;          /**< Path to the audio file relative to the project root. */
    char*       group;             /**< Logical group (e.g., gameplay cycle). */
    char*       eventName;         /**< Optional event tag that triggers this music. */
    MusicUsage  usage;             /**< Declared usage category. */
    bool        loop;              /**< Whether the track should loop until stopped. */
    int         loopCount;         /**< Specific loop count, -1 to use library default. */
    float       defaultVolume;     /**< Preferred playback volume (0.0 - 1.0). */
    float       defaultFadeIn;     /**< Recommended fade-in duration in seconds. */
    float       defaultFadeOut;    /**< Recommended fade-out duration in seconds. */
    float       crossfadeLead;     /**< Lead time before end to begin crossfading. */
    float       cueOffset;         /**< Optional offset in seconds from which to start playback. */
} MusicDefinition;

/**
 * @brief Loads music definitions from an .stv file and returns a dynamic array.
 *
 * The caller becomes owner of the returned array and must release it with
 * music_loader_free(). The function logs parsing errors to stderr and skips
 * malformed sections gracefully.
 *
 * @param path     Path to the .stv configuration file.
 * @param outCount Receives the number of successfully parsed definitions.
 * @return Dynamically allocated array of MusicDefinition on success, or NULL on failure.
 */
MusicDefinition* music_loader_load(const char* path, int* outCount);

/**
 * @brief Releases memory allocated for an array of MusicDefinition.
 *
 * @param defs  Pointer to the array previously returned by music_loader_load().
 * @param count Number of elements inside the array.
 */
void music_loader_free(MusicDefinition* defs, int count);

#endif /* MUSIC_LOADER_H */
