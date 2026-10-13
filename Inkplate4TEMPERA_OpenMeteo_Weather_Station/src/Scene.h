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

// Draw the static "what it looks like outside" picture as a framed panel in (x, y, w, h).
// By day it is a white panel with dark drawing; at night it is a black panel with white drawing
// (moon, stars, white drops). Deterministic, so the picture is stable between wakes.
void drawBackdrop(Inkplate &display, SceneKind kind, bool night, int x, int y, int w, int h);

#endif
