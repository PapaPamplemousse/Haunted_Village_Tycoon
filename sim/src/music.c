/**
 * @file music.c
 * @brief Implements the streaming music system with fades and event overrides.
 */

#include "music.h"

#include "music_loader.h"
#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifndef MUSIC_LAYER_COUNT
#define MUSIC_LAYER_COUNT 2
#endif

/**
 * @brief Local strdup helper to remain portable.
 */
static char* str_dup(const char* s)
{
    if (!s)
        return NULL;
    size_t len  = strlen(s) + 1;
    char*  copy = (char*)malloc(len);
    if (copy)
        memcpy(copy, s, len);
    return copy;
}

typedef struct LoadedTrack
{
    MusicDefinition* meta;
    Music            handle;
    bool             valid;
} LoadedTrack;

typedef struct MusicLayer
{
    int   clipIndex;
    float volume;
    float startVolume;
    float targetVolume;
    float fadeDuration;
    float fadeElapsed;
    bool  active;
    bool  inTransition;
    bool  stopWhenSilent;
    bool  isEventLayer;
} MusicLayer;

typedef struct MusicSystemState
{
    bool                initialized;
    MusicDefinition*    defs;
    int                 defCount;
    LoadedTrack*        tracks;
    int                 trackCount;
    MusicLayer          layers[MUSIC_LAYER_COUNT];
    float               masterVolume;
    float               gameplayCrossfadeLead;
    char*               gameplayGroup;
    int*                gameplayOrder;
    int                 gameplayCount;
    int                 gameplayCursor;
    int                 currentGameplayClip;
    bool                eventActive;
    int                 eventClipIndex;
    MusicTransitionType resumeTransition;
    float               resumeDuration;
    int                 pendingClipIndex;
    float               pendingFadeDuration;
    bool                pendingIsEvent;
    bool                pendingStartAfterFade;
    bool                transitionInProgress;
} MusicSystemState;

static MusicSystemState G_MUSIC = {0};

// -----------------------------------------------------------------------
// Internal helpers
// -----------------------------------------------------------------------

static void reset_layer(MusicLayer* layer)
{
    if (!layer)
        return;
    layer->clipIndex      = -1;
    layer->volume         = 0.0f;
    layer->startVolume    = 0.0f;
    layer->targetVolume   = 0.0f;
    layer->fadeDuration   = 0.0f;
    layer->fadeElapsed    = 0.0f;
    layer->active         = false;
    layer->inTransition   = false;
    layer->stopWhenSilent = true;
    layer->isEventLayer   = false;
}

static float clamp_volume(float v)
{
    if (v < 0.0f)
        return 0.0f;
    if (v > 1.0f)
        return 1.0f;
    return v;
}

typedef struct MusicLoopCounter
{
    Music                    music;
    int                      remainingLoops;
    struct MusicLoopCounter* next;
} MusicLoopCounter;

static MusicLoopCounter* g_loopList = NULL;

// Find loop counter entry for given music handle
static MusicLoopCounter* find_loop_entry(Music m)
{
    for (MusicLoopCounter* n = g_loopList; n; n = n->next)
        if (n->music.ctxData == m.ctxData)
            return n;
    return NULL;
}

// Add or update a loop counter
void SetMusicLoopCount(Music m, int count)
{
    MusicLoopCounter* entry = find_loop_entry(m);
    if (!entry)
    {
        entry        = (MusicLoopCounter*)malloc(sizeof(MusicLoopCounter));
        entry->music = m;
        entry->next  = g_loopList;
        g_loopList   = entry;
    }
    entry->remainingLoops = count;
}

// Internal update hook (should be called from update_layer)
static void update_music_loops(Music m)
{
    MusicLoopCounter* entry = find_loop_entry(m);
    if (!entry)
        return;

    float total  = GetMusicTimeLength(m);
    float played = GetMusicTimePlayed(m);

    // When track reaches end
    if (total > 0.0f && played >= total - 0.01f)
    {
        if (entry->remainingLoops > 1)
        {
            entry->remainingLoops--;
            SeekMusicStream(m, 0.0f);
        }
        else
        {
            entry->remainingLoops = 0;
            StopMusicStream(m);
        }
    }
}

// Cleanup all entries (call this in music_system_shutdown)
static void clear_music_loops(void)
{
    MusicLoopCounter* n = g_loopList;
    while (n)
    {
        MusicLoopCounter* next = n->next;
        free(n);
        n = next;
    }
    g_loopList = NULL;
}
static LoadedTrack* get_track(int clipIndex)
{
    if (clipIndex < 0 || clipIndex >= G_MUSIC.trackCount)
        return NULL;
    return &G_MUSIC.tracks[clipIndex];
}

static MusicLayer* find_layer_for_clip(int clipIndex)
{
    for (int i = 0; i < MUSIC_LAYER_COUNT; ++i)
        if (G_MUSIC.layers[i].active && G_MUSIC.layers[i].clipIndex == clipIndex)
            return &G_MUSIC.layers[i];
    return NULL;
}

static MusicLayer* acquire_layer(void)
{
    for (int i = 0; i < MUSIC_LAYER_COUNT; ++i)
        if (!G_MUSIC.layers[i].active)
            return &G_MUSIC.layers[i];
    // Fall back to the layer with the lowest volume.
    int   lowestIndex  = 0;
    float lowestVolume = G_MUSIC.layers[0].volume;
    for (int i = 1; i < MUSIC_LAYER_COUNT; ++i)
    {
        if (G_MUSIC.layers[i].volume < lowestVolume)
        {
            lowestVolume = G_MUSIC.layers[i].volume;
            lowestIndex  = i;
        }
    }
    return &G_MUSIC.layers[lowestIndex];
}

static void stop_layer(MusicLayer* layer, bool forceStop)
{
    if (!layer || !layer->active)
        return;

    LoadedTrack* track = get_track(layer->clipIndex);
    if (track && track->valid)
    {
        if (forceStop || layer->volume <= 0.001f)
            StopMusicStream(track->handle);
    }

    reset_layer(layer);
}

static void apply_volume(const MusicLayer* layer)
{
    if (!layer || !layer->active)
        return;
    LoadedTrack* track = get_track(layer->clipIndex);
    if (!track || !track->valid)
        return;

    float baseVolume  = (track->meta && track->meta->defaultVolume > 0.0f) ? track->meta->defaultVolume : 1.0f;
    float finalVolume = clamp_volume(layer->volume) * clamp_volume(G_MUSIC.masterVolume) * clamp_volume(baseVolume);
    SetMusicVolume(track->handle, finalVolume);
}

static void start_layer(MusicLayer* layer, int clipIndex, float startVolume, float targetVolume, float fadeDuration, bool isEventLayer)
{
    if (!layer)
        return;

    LoadedTrack* track = get_track(clipIndex);
    if (!track || !track->valid || !track->meta)
        return;

    StopMusicStream(track->handle);
    PlayMusicStream(track->handle);

    if (track->meta->cueOffset > 0.0f)
        SeekMusicStream(track->handle, track->meta->cueOffset);

    layer->clipIndex      = clipIndex;
    layer->volume         = clamp_volume(startVolume);
    layer->startVolume    = layer->volume;
    layer->targetVolume   = clamp_volume(targetVolume);
    layer->fadeDuration   = (fadeDuration < 0.0f) ? 0.0f : fadeDuration;
    layer->fadeElapsed    = 0.0f;
    layer->active         = true;
    layer->inTransition   = (layer->fadeDuration > 0.0f && fabsf(layer->targetVolume - layer->startVolume) > 0.0001f);
    layer->stopWhenSilent = true;
    layer->isEventLayer   = isEventLayer;

    if (track->meta->loop)
        track->handle.looping = true;
    else
        track->handle.looping = false;

    if (track->meta->loopCount >= 0)
        SetMusicLoopCount(track->handle, track->meta->loopCount);

    apply_volume(layer);
}

static void begin_fade_out(MusicLayer* layer, float duration)
{
    if (!layer || !layer->active)
        return;
    layer->startVolume  = layer->volume;
    layer->targetVolume = 0.0f;
    layer->fadeDuration = (duration < 0.0f) ? 0.0f : duration;
    layer->fadeElapsed  = 0.0f;
    layer->inTransition = (layer->fadeDuration > 0.0f && layer->startVolume > 0.0f);
}

static void update_layer(MusicLayer* layer, float dt)
{
    if (!layer || !layer->active)
        return;

    LoadedTrack* track = get_track(layer->clipIndex);
    if (!track || !track->valid)
    {
        reset_layer(layer);
        return;
    }

    UpdateMusicStream(track->handle);

    if (layer->inTransition)
    {
        layer->fadeElapsed += dt;
        float t = (layer->fadeDuration > 0.0f) ? (layer->fadeElapsed / layer->fadeDuration) : 1.0f;
        if (t >= 1.0f)
        {
            t                   = 1.0f;
            layer->inTransition = false;
        }
        layer->volume = layer->startVolume + (layer->targetVolume - layer->startVolume) * t;
    }
    else
    {
        layer->volume = layer->targetVolume;
    }

    apply_volume(layer);

    if (!layer->inTransition && layer->volume <= 0.001f && layer->targetVolume <= 0.0f && layer->stopWhenSilent)
        stop_layer(layer, true);
}

static void stop_all_layers(void)
{
    for (int i = 0; i < MUSIC_LAYER_COUNT; ++i)
        stop_layer(&G_MUSIC.layers[i], true);
}

static float positive_or_default(float value, float fallback)
{
    return (value > 0.0f) ? value : fallback;
}

static void rebuild_gameplay_playlist(const char* groupName)
{
    free(G_MUSIC.gameplayOrder);
    G_MUSIC.gameplayOrder       = NULL;
    G_MUSIC.gameplayCount       = 0;
    G_MUSIC.gameplayCursor      = 0;
    G_MUSIC.currentGameplayClip = -1;

    if (!G_MUSIC.defs || G_MUSIC.defCount == 0)
        return;

    int capacity          = 8;
    G_MUSIC.gameplayOrder = (int*)malloc(sizeof(int) * (size_t)capacity);
    if (!G_MUSIC.gameplayOrder)
        return;

    for (int i = 0; i < G_MUSIC.defCount; ++i)
    {
        MusicDefinition* def = &G_MUSIC.defs[i];
        if (!def)
            continue;
        if (def->usage != MUSIC_USAGE_GAMEPLAY)
            continue;

        bool belongs = false;
        if (groupName && def->group)
            belongs = (strcasecmp(def->group, groupName) == 0);
        else if (!groupName && (!def->group || def->group[0] == '\0'))
            belongs = true;
        else if (!groupName)
            belongs = true; // include all gameplay tracks when group is unspecified.

        if (!belongs)
            continue;

        if (G_MUSIC.gameplayCount >= capacity)
        {
            capacity *= 2;
            int* resized = (int*)realloc(G_MUSIC.gameplayOrder, sizeof(int) * (size_t)capacity);
            if (!resized)
                break;
            G_MUSIC.gameplayOrder = resized;
        }

        G_MUSIC.gameplayOrder[G_MUSIC.gameplayCount++] = i;
    }

    if (G_MUSIC.gameplayCount == 0)
    {
        free(G_MUSIC.gameplayOrder);
        G_MUSIC.gameplayOrder = NULL;
    }
}

static int find_event_clip(const char* eventName)
{
    if (!eventName || !G_MUSIC.defs)
        return -1;
    for (int i = 0; i < G_MUSIC.defCount; ++i)
    {
        MusicDefinition* def = &G_MUSIC.defs[i];
        if (!def || def->usage != MUSIC_USAGE_EVENT)
            continue;
        if (def->eventName && strcasecmp(def->eventName, eventName) == 0)
            return i;
    }
    return -1;
}

static void mark_transition(bool active)
{
    G_MUSIC.transitionInProgress = active;
}

static void start_clip_internal(int clipIndex, MusicTransitionType transition, float durationSeconds, bool isEvent)
{
    LoadedTrack* track = get_track(clipIndex);
    if (!track || !track->valid || !track->meta)
        return;

    float fadeIn = positive_or_default(durationSeconds, track->meta->defaultFadeIn);

    switch (transition)
    {
        case MUSIC_TRANSITION_IMMEDIATE:
        {
            stop_all_layers();
            MusicLayer* layer = &G_MUSIC.layers[0];
            reset_layer(layer);
            start_layer(layer, clipIndex, 1.0f, 1.0f, 0.0f, isEvent);
            mark_transition(false);
            break;
        }
        case MUSIC_TRANSITION_CROSSFADE:
        {
            // Fade out every active layer that does not match the new clip.
            for (int i = 0; i < MUSIC_LAYER_COUNT; ++i)
            {
                MusicLayer* layer = &G_MUSIC.layers[i];
                if (!layer->active)
                    continue;
                if (layer->clipIndex == clipIndex)
                {
                    layer->targetVolume = 1.0f;
                    layer->startVolume  = layer->volume;
                    layer->fadeElapsed  = 0.0f;
                    layer->fadeDuration = fadeIn;
                    layer->inTransition = (fadeIn > 0.0f && fabsf(layer->targetVolume - layer->startVolume) > 0.0001f);
                    layer->isEventLayer = isEvent;
                    continue;
                }
                LoadedTrack* oldTrack  = get_track(layer->clipIndex);
                float        layerFade = positive_or_default(durationSeconds, (oldTrack && oldTrack->meta) ? oldTrack->meta->defaultFadeOut : track->meta->defaultFadeOut);
                begin_fade_out(layer, layerFade);
            }

            MusicLayer* existing = find_layer_for_clip(clipIndex);
            if (!existing)
            {
                MusicLayer* layer = acquire_layer();
                if (layer->active)
                    stop_layer(layer, true);
                start_layer(layer, clipIndex, 0.0f, 1.0f, fadeIn, isEvent);
            }

            mark_transition(true);
            break;
        }
        case MUSIC_TRANSITION_FADE:
        default:
        {
            for (int i = 0; i < MUSIC_LAYER_COUNT; ++i)
            {
                MusicLayer*  layer     = &G_MUSIC.layers[i];
                LoadedTrack* oldTrack  = get_track(layer->clipIndex);
                float        layerFade = positive_or_default(durationSeconds, (oldTrack && oldTrack->meta) ? oldTrack->meta->defaultFadeOut : track->meta->defaultFadeOut);
                begin_fade_out(layer, layerFade);
            }

            G_MUSIC.pendingClipIndex      = clipIndex;
            G_MUSIC.pendingFadeDuration   = fadeIn;
            G_MUSIC.pendingIsEvent        = isEvent;
            G_MUSIC.pendingStartAfterFade = true;
            mark_transition(true);
            break;
        }
    }

    if (isEvent)
    {
        G_MUSIC.eventActive    = true;
        G_MUSIC.eventClipIndex = clipIndex;
    }
    else
    {
        G_MUSIC.currentGameplayClip = clipIndex;
    }
}

static void try_start_pending_clip(void)
{
    if (!G_MUSIC.pendingStartAfterFade || G_MUSIC.pendingClipIndex < 0)
        return;

    bool anyActive = false;
    for (int i = 0; i < MUSIC_LAYER_COUNT; ++i)
        anyActive |= G_MUSIC.layers[i].active;

    if (anyActive)
        return;

    LoadedTrack* track = get_track(G_MUSIC.pendingClipIndex);
    if (!track || !track->valid || !track->meta)
    {
        G_MUSIC.pendingClipIndex      = -1;
        G_MUSIC.pendingStartAfterFade = false;
        mark_transition(false);
        return;
    }

    MusicLayer* layer = &G_MUSIC.layers[0];
    reset_layer(layer);
    start_layer(layer, G_MUSIC.pendingClipIndex, 0.0f, 1.0f, G_MUSIC.pendingFadeDuration, G_MUSIC.pendingIsEvent);

    if (G_MUSIC.pendingIsEvent)
    {
        G_MUSIC.eventActive    = true;
        G_MUSIC.eventClipIndex = G_MUSIC.pendingClipIndex;
    }
    else
    {
        G_MUSIC.currentGameplayClip = G_MUSIC.pendingClipIndex;
    }

    G_MUSIC.pendingClipIndex      = -1;
    G_MUSIC.pendingStartAfterFade = false;
    mark_transition(false);
}

static void schedule_next_gameplay(MusicTransitionType transition, float duration)
{
    if (G_MUSIC.gameplayCount == 0)
        return;

    int nextIndex          = G_MUSIC.gameplayOrder[G_MUSIC.gameplayCursor % G_MUSIC.gameplayCount];
    G_MUSIC.gameplayCursor = (G_MUSIC.gameplayCursor + 1) % G_MUSIC.gameplayCount;
    start_clip_internal(nextIndex, transition, duration, false);
}

static float compute_time_remaining(int clipIndex)
{
    LoadedTrack* track = get_track(clipIndex);
    if (!track || !track->valid)
        return -1.0f;

    float length = GetMusicTimeLength(track->handle);
    float played = GetMusicTimePlayed(track->handle);
    if (length <= 0.0f)
        return -1.0f;
    return length - played;
}

static bool is_layer_active_for_clip(int clipIndex)
{
    return (find_layer_for_clip(clipIndex) != NULL);
}

// -----------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------

bool music_system_init(const char* configPath, const char* gameplayGroup)
{
    if (G_MUSIC.initialized)
        return true;

    if (!IsAudioDeviceReady())
        InitAudioDevice();

    G_MUSIC.defs = music_loader_load(configPath, &G_MUSIC.defCount);
    if (!G_MUSIC.defs || G_MUSIC.defCount == 0)
    {
        fprintf(stderr, "Warning: No music definitions were loaded. Music disabled.\n");
        return false;
    }

    G_MUSIC.tracks = (LoadedTrack*)calloc((size_t)G_MUSIC.defCount, sizeof(LoadedTrack));
    if (!G_MUSIC.tracks)
    {
        music_loader_free(G_MUSIC.defs, G_MUSIC.defCount);
        G_MUSIC.defs     = NULL;
        G_MUSIC.defCount = 0;
        fprintf(stderr, "Error: Out of memory allocating music tracks.\n");
        return false;
    }

    G_MUSIC.trackCount            = G_MUSIC.defCount;
    G_MUSIC.masterVolume          = 1.0f;
    G_MUSIC.gameplayCrossfadeLead = 4.0f;
    G_MUSIC.gameplayGroup         = NULL;
    G_MUSIC.gameplayOrder         = NULL;
    G_MUSIC.gameplayCount         = 0;
    G_MUSIC.gameplayCursor        = 0;
    G_MUSIC.currentGameplayClip   = -1;
    G_MUSIC.eventActive           = false;
    G_MUSIC.eventClipIndex        = -1;
    G_MUSIC.resumeTransition      = MUSIC_TRANSITION_CROSSFADE;
    G_MUSIC.resumeDuration        = 1.5f;
    G_MUSIC.pendingClipIndex      = -1;
    G_MUSIC.pendingFadeDuration   = 0.0f;
    G_MUSIC.pendingIsEvent        = false;
    G_MUSIC.pendingStartAfterFade = false;
    G_MUSIC.transitionInProgress  = false;

    for (int i = 0; i < MUSIC_LAYER_COUNT; ++i)
        reset_layer(&G_MUSIC.layers[i]);

    if (gameplayGroup)
        G_MUSIC.gameplayGroup = str_dup(gameplayGroup);

    for (int i = 0; i < G_MUSIC.defCount; ++i)
    {
        G_MUSIC.tracks[i].meta   = &G_MUSIC.defs[i];
        G_MUSIC.tracks[i].handle = LoadMusicStream(G_MUSIC.defs[i].filePath ? G_MUSIC.defs[i].filePath : "");
        G_MUSIC.tracks[i].valid  = (G_MUSIC.tracks[i].handle.ctxData != NULL);
        if (!G_MUSIC.tracks[i].valid)
        {
            fprintf(stderr, "Warning: Failed to load music stream: %s\n", G_MUSIC.defs[i].filePath ? G_MUSIC.defs[i].filePath : "(null)");
            continue;
        }

        G_MUSIC.tracks[i].handle.looping = G_MUSIC.defs[i].loop;
        if (G_MUSIC.defs[i].loopCount >= 0)
            SetMusicLoopCount(G_MUSIC.tracks[i].handle, G_MUSIC.defs[i].loopCount);
    }

    rebuild_gameplay_playlist(G_MUSIC.gameplayGroup);
    if (G_MUSIC.gameplayCount > 0)
    {
        G_MUSIC.gameplayCursor = 0;
        schedule_next_gameplay(MUSIC_TRANSITION_IMMEDIATE, 0.0f);
    }

    if (IsAudioDeviceReady())
        SetMasterVolume(1.0f);
    G_MUSIC.initialized = true;
    return true;
}

void music_system_shutdown(void)
{
    if (!G_MUSIC.initialized)
        return;

    stop_all_layers();

    if (G_MUSIC.tracks)
    {
        for (int i = 0; i < G_MUSIC.trackCount; ++i)
        {
            if (G_MUSIC.tracks[i].valid)
                UnloadMusicStream(G_MUSIC.tracks[i].handle);
        }
        free(G_MUSIC.tracks);
        G_MUSIC.tracks = NULL;
    }

    music_loader_free(G_MUSIC.defs, G_MUSIC.defCount);
    G_MUSIC.defs     = NULL;
    G_MUSIC.defCount = 0;

    free(G_MUSIC.gameplayGroup);
    G_MUSIC.gameplayGroup = NULL;

    free(G_MUSIC.gameplayOrder);
    G_MUSIC.gameplayOrder = NULL;
    G_MUSIC.gameplayCount = 0;

    G_MUSIC.initialized         = false;
    G_MUSIC.eventActive         = false;
    G_MUSIC.eventClipIndex      = -1;
    G_MUSIC.currentGameplayClip = -1;

    if (IsAudioDeviceReady())
        CloseAudioDevice();
}

void music_system_update(float deltaTime)
{
    if (!G_MUSIC.initialized || !IsAudioDeviceReady())
        return;

    for (int i = 0; i < MUSIC_LAYER_COUNT; ++i)
        update_layer(&G_MUSIC.layers[i], deltaTime);

    if (G_MUSIC.pendingStartAfterFade)
        try_start_pending_clip();

    bool anyTransition = false;
    for (int i = 0; i < MUSIC_LAYER_COUNT; ++i)
        anyTransition |= G_MUSIC.layers[i].inTransition;
    mark_transition(anyTransition);

    if (G_MUSIC.eventActive)
    {
        if (!is_layer_active_for_clip(G_MUSIC.eventClipIndex))
        {
            G_MUSIC.eventActive    = false;
            G_MUSIC.eventClipIndex = -1;
            if (G_MUSIC.gameplayCount > 0)
                schedule_next_gameplay(G_MUSIC.resumeTransition, G_MUSIC.resumeDuration);
        }
        return;
    }

    if (G_MUSIC.gameplayCount == 0 || G_MUSIC.currentGameplayClip < 0)
        return;

    if (!is_layer_active_for_clip(G_MUSIC.currentGameplayClip))
    {
        schedule_next_gameplay(MUSIC_TRANSITION_IMMEDIATE, 0.0f);
        return;
    }

    if (G_MUSIC.transitionInProgress)
        return;

    float        remaining    = compute_time_remaining(G_MUSIC.currentGameplayClip);
    LoadedTrack* currentTrack = get_track(G_MUSIC.currentGameplayClip);
    float        leadSetting  = G_MUSIC.gameplayCrossfadeLead;
    if (currentTrack && currentTrack->meta && currentTrack->meta->crossfadeLead > 0.0f)
        leadSetting = currentTrack->meta->crossfadeLead;
    float lead = fmaxf(0.2f, leadSetting);
    if (remaining > 0.0f && remaining <= lead)
        schedule_next_gameplay(MUSIC_TRANSITION_CROSSFADE, 0.0f);
}

void music_system_set_gameplay_crossfade_lead(float seconds)
{
    G_MUSIC.gameplayCrossfadeLead = (seconds > 0.1f) ? seconds : 0.1f;
}

bool music_system_set_gameplay_group(const char* groupName, bool restartImmediately)
{
    if (!G_MUSIC.initialized)
        return false;

    free(G_MUSIC.gameplayGroup);
    G_MUSIC.gameplayGroup = groupName ? str_dup(groupName) : NULL;

    rebuild_gameplay_playlist(G_MUSIC.gameplayGroup);

    if (restartImmediately && G_MUSIC.gameplayCount > 0)
    {
        G_MUSIC.gameplayCursor = 0;
        schedule_next_gameplay(MUSIC_TRANSITION_FADE, 1.0f);
    }

    return (G_MUSIC.gameplayCount > 0);
}

void music_system_force_next(MusicTransitionType transition, float durationSeconds)
{
    if (!G_MUSIC.initialized || G_MUSIC.gameplayCount == 0)
        return;
    schedule_next_gameplay(transition, durationSeconds);
}

bool music_system_trigger_event(const char* eventName, MusicTransitionType transition, float durationSeconds)
{
    if (!G_MUSIC.initialized || !eventName)
        return false;

    int clipIndex = find_event_clip(eventName);
    if (clipIndex < 0)
        return false;

    G_MUSIC.resumeTransition = (transition == MUSIC_TRANSITION_IMMEDIATE) ? MUSIC_TRANSITION_FADE : transition;
    G_MUSIC.resumeDuration   = positive_or_default(durationSeconds, 1.0f);

    start_clip_internal(clipIndex, transition, durationSeconds, true);
    return true;
}

void music_system_clear_event(MusicTransitionType transition, float durationSeconds)
{
    if (!G_MUSIC.initialized)
        return;

    if (!G_MUSIC.eventActive)
        return;

    G_MUSIC.eventActive    = false;
    G_MUSIC.eventClipIndex = -1;

    if (G_MUSIC.gameplayCount == 0)
        return;

    schedule_next_gameplay(transition, durationSeconds);
}

void music_system_set_master_volume(float volume)
{
    G_MUSIC.masterVolume = clamp_volume(volume);
    for (int i = 0; i < MUSIC_LAYER_COUNT; ++i)
        apply_volume(&G_MUSIC.layers[i]);
}

bool music_system_is_event_active(void)
{
    return G_MUSIC.eventActive;
}
