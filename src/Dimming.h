#pragma once

#include "LightModel.h"

Rgb scaleColor(Rgb color, int level);
unsigned int scaleBrightness(unsigned int brightness, unsigned int minimum, int level);
LightState renderAtLevel(const LightState& base, int level);
LightState withAccent(const LightState& base, Rgb accent);
