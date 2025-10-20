
#ifndef CAMERA_H
#define CAMERA_H

#include "raylib.h"

/**
 * @brief Initializes and returns a Camera2D object.
 *
 * @return Camera2D The initialized camera.
 */
Camera2D init_camera(void);

/**
 * @brief Updates the camera position based on user input.
 *
 * @param camera Pointer to the Camera2D object to be updated.
 */
void update_camera(Camera2D *camera);

#endif
