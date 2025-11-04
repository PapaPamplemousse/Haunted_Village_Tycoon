/**
 * @file ui_theme.c
 * @brief Loads the shared UI texture atlas and exposes handy rectangles.
 */

#include "ui_theme.h"

#include <string.h>

static UiTheme G_THEME;
static bool    G_THEME_READY = false;

static NPatchInfo make_npatch(Rectangle source, int border)
{
    NPatchInfo patch = {
        .source = source,
        .left   = border,
        .top    = border,
        .right  = border,
        .bottom = border,
        .layout = NPATCH_NINE_PATCH,
    };
    return patch;
}

static NPatchInfo make_npatch_custom(Rectangle source, int left, int top, int right, int bottom)
{
    NPatchInfo patch = {
        .source = source,
        .left   = left,
        .top    = top,
        .right  = right,
        .bottom = bottom,
        .layout = NPATCH_NINE_PATCH,
    };
    return patch;
}

bool ui_theme_init(const char* atlasPath)
{
    if (G_THEME_READY)
        return true;

    if (!atlasPath)
        return false;

    Image atlas = LoadImage(atlasPath);
    if (atlas.data == NULL)
        return false;

    memset(&G_THEME, 0, sizeof(G_THEME));

    G_THEME.atlas = LoadTextureFromImage(atlas);
    UnloadImage(atlas);
    if (G_THEME.atlas.id == 0)
        return false;

    SetTextureFilter(G_THEME.atlas, TEXTURE_FILTER_POINT);
    SetTextureWrap(G_THEME.atlas, TEXTURE_WRAP_CLAMP);

    // --- Panels -----------------------------------------------------------------
    G_THEME.panelLarge  = make_npatch_custom((Rectangle){932, 289, 87, 78}, 22, 26, 22, 26);
    G_THEME.panelMedium = make_npatch_custom((Rectangle){932, 369, 87, 60}, 18, 20, 18, 20);
    G_THEME.panelSmall  = make_npatch_custom((Rectangle){897, 241, 92, 45}, 18, 20, 18, 20);

    // --- Buttons ----------------------------------------------------------------
    G_THEME.buttonNormal  = make_npatch_custom((Rectangle){163, 100, 90, 27}, 18, 10, 18, 10);
    G_THEME.buttonHover   = make_npatch_custom((Rectangle){163, 100, 90, 27}, 18, 10, 18, 10);
    G_THEME.buttonPressed = make_npatch_custom((Rectangle){163, 130, 90, 27}, 18, 10, 18, 10);

    // --- Tabs -------------------------------------------------------------------
    G_THEME.tabActive   = make_npatch_custom((Rectangle){259, 101, 90, 25}, 16, 8, 16, 8);
    G_THEME.tabInactive = make_npatch_custom((Rectangle){259, 132, 90, 25}, 16, 8, 16, 8);

    // --- Frames / Highlights ----------------------------------------------------
    G_THEME.slotFrame     = (Rectangle){320, 560, 31, 31};
    G_THEME.tileHighlight = (Rectangle){516, 324, 23, 24};
    G_THEME.badgeRound    = (Rectangle){615, 39, 48, 16};

    // --- Palette ----------------------------------------------------------------
    G_THEME.textPrimary   = (Color){235, 214, 214, 255};
    G_THEME.textSecondary = (Color){180, 150, 150, 255};
    G_THEME.accent        = (Color){180, 60, 60, 255};
    G_THEME.accentBright  = (Color){230, 120, 120, 255};
    G_THEME.overlayDim    = (Color){0, 0, 0, 180};

    G_THEME_READY = true;
    return true;
}

void ui_theme_shutdown(void)
{
    if (!G_THEME_READY)
        return;

    if (G_THEME.atlas.id != 0)
        UnloadTexture(G_THEME.atlas);

    memset(&G_THEME, 0, sizeof(G_THEME));
    G_THEME_READY = false;
}

bool ui_theme_is_ready(void)
{
    return G_THEME_READY && (G_THEME.atlas.id != 0);
}

const UiTheme* ui_theme_get(void)
{
    return ui_theme_is_ready() ? &G_THEME : NULL;
}
