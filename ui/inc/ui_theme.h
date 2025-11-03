/**
 * @file ui_theme.h
 * @brief Declares helpers to load and query the shared UI texture atlas.
 */
#ifndef UI_THEME_H
#define UI_THEME_H

#include <stdbool.h>
#include "raylib.h"

/**
 * @brief Centralizes the atlas rectangles and palette used by the UI.
 */
typedef struct UiTheme
{
    Texture2D atlas;            /**< Loaded texture atlas (pixel art UI sheet). */

    NPatchInfo panelLarge;      /**< Generic large panel used for inventories / dialogs. */
    NPatchInfo panelMedium;     /**< Medium-sized panel, ideal for pause/settings boxes. */
    NPatchInfo panelSmall;      /**< Small badges / header strips. */

    NPatchInfo buttonNormal;    /**< Default button background. */
    NPatchInfo buttonHover;     /**< Hovered button variant. */
    NPatchInfo buttonPressed;   /**< Pressed/active button variant. */

    NPatchInfo tabActive;       /**< Active tab badge. */
    NPatchInfo tabInactive;     /**< Inactive tab badge. */

    Rectangle slotFrame;        /**< 32x32 slot frame for inventory cells. */
    Rectangle tileHighlight;    /**< 65x63 frame used to highlight world selections. */
    Rectangle badgeRound;       /**< Small badge (e.g., to show Tab toggle). */

    Color     textPrimary;      /**< Main text color. */
    Color     textSecondary;    /**< Muted text color. */
    Color     accent;           /**< Accent color for glyphs / icons. */
    Color     accentBright;     /**< Strong accent for highlights. */
    Color     overlayDim;       /**< Semi-transparent overlay when modal UI is open. */
} UiTheme;

/**
 * @brief Loads the UI atlas and prepares rectangles / palette values.
 *
 * @param atlasPath Path to the `ui.png` spritesheet.
 * @return true on success, false otherwise.
 */
bool ui_theme_init(const char* atlasPath);

/**
 * @brief Releases the atlas texture and resets theme data.
 */
void ui_theme_shutdown(void);

/**
 * @brief Checks whether the theme is ready for drawing.
 */
bool ui_theme_is_ready(void);

/**
 * @brief Provides read-only access to the theme data.
 *
 * @return Pointer to the internal theme descriptor (valid while initialized), or NULL.
 */
const UiTheme* ui_theme_get(void);

#endif /* UI_THEME_H */
