/**
 * @file camera.h
 * @brief Provides top-down camera initialization and logic, decoupled from input.
 */

#ifndef CAMERA_H
#define CAMERA_H

#include <raylib.h>
#include "input.h" // Needed for CameraInput

#define ZOOM_MIN 0.9f
#define ZOOM_MAX 2.5f

/**
 * @brief Initializes a top-down camera centered on the middle of the map.
 *
 * The camera is configured to fit the visible area and supports adaptive fullscreen behavior.
 *
 * @return A fully initialized Camera2D.
 */
Camera2D init_camera(void);

/**
 * @brief Updates the camera position and zoom according to user input data.
 *
 * Unlike previous versions, this function does not read input directly.
 * It only applies movement and zoom based on the provided CameraInput.
 *
 * @param[in,out] camera Pointer to the active camera.
 * @param[in] input Pointer to a CameraInput structure describing user intent.
 */
void update_camera(Camera2D* camera, const CameraInput* input);

#endif // CAMERA_H
