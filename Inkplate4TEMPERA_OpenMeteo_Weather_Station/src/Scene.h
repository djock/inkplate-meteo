#ifndef SCENE_H
#define SCENE_H

#include "Inkplate.h"

enum class SceneKind
{
    Clear,
    PartlyCloudy,
    Cloudy,
    Fog,
    Rain,
    Snow,
    Storm
};

// Pick the backdrop for an Open-Meteo weather code.
SceneKind sceneForWeather(int weatherCode);

// Draw the static "what it looks like outside" picture in (x, y, w, h): dark drawing on white, always.
// At night the sun becomes a moon with stars. Deterministic, so the picture is stable between wakes.
void drawBackdrop(Inkplate &display, SceneKind kind, bool night, int x, int y, int w, int h);

#endif
