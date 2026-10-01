#pragma once

#include <vector>

using Rgb = unsigned int;

inline Rgb makeRgb(unsigned int red, unsigned int green, unsigned int blue)
{
    return (blue << 16) | (green << 8) | red;
}

inline unsigned int rgbRed(Rgb color)
{
    return color & 0xFF;
}

inline unsigned int rgbGreen(Rgb color)
{
    return (color >> 8) & 0xFF;
}

inline unsigned int rgbBlue(Rgb color)
{
    return (color >> 16) & 0xFF;
}

enum class ColorMode
{
    None = 0,
    PerLed = 1,
    ModeSpecific = 2,
    Random = 3,
};

struct ModeState
{
    int index = -1;
    ColorMode colorMode = ColorMode::None;
    bool hasBrightness = false;
    unsigned int brightness = 0;
    unsigned int brightnessMin = 0;
    unsigned int brightnessMax = 0;
    std::vector<Rgb> colors;
};

struct ZoneState
{
    ModeState mode;
    std::vector<Rgb> leds;
};

struct LightState
{
    ModeState mode;
    std::vector<ZoneState> zones;
};

inline bool operator==(const ModeState& a, const ModeState& b)
{
    return a.index == b.index && a.colorMode == b.colorMode && a.hasBrightness == b.hasBrightness && a.brightness == b.brightness
        && a.brightnessMin == b.brightnessMin && a.brightnessMax == b.brightnessMax && a.colors == b.colors;
}

inline bool operator!=(const ModeState& a, const ModeState& b)
{
    return !(a == b);
}

inline bool operator==(const ZoneState& a, const ZoneState& b)
{
    return a.mode == b.mode && a.leds == b.leds;
}

inline bool operator!=(const ZoneState& a, const ZoneState& b)
{
    return !(a == b);
}

inline bool operator==(const LightState& a, const LightState& b)
{
    return a.mode == b.mode && a.zones == b.zones;
}

inline bool operator!=(const LightState& a, const LightState& b)
{
    return !(a == b);
}
