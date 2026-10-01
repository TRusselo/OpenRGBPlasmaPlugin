#pragma once

#include <optional>
#include <vector>

#include "LightModel.h"

Rgb scaleColor(Rgb color, int level);
unsigned int scaleBrightness(unsigned int brightness, unsigned int minimum, int level);
LightState renderAtLevel(const LightState& base, int level);
LightState withAccent(const LightState& base, Rgb accent);
bool isUnlit(const LightState& state);
std::optional<Rgb> litColor(const LightState& state);
std::optional<Rgb> blendColors(const std::vector<Rgb>& colors);
