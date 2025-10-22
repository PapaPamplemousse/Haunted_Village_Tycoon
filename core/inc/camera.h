#ifndef CAMERA_H
#define CAMERA_H

#include <raylib.h>

// Initialise la caméra en vue du dessus centrée sur la moitié de la carte
Camera2D init_camera(void);

// Met à jour la caméra (déplacement, zoom)
void update_camera(Camera2D* camera);

#endif // CAMERA_H
