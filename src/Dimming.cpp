#include "Dimming.h"

#include <algorithm>

namespace
{
unsigned int scaleChannel(unsigned int value, int level)
{
    return (value * (unsigned int)level + 50) / 100;
}

void scaleMode(ModeState& mode, int level)
{
    if(mode.colorMode == ColorMode::ModeSpecific)
    {
        for(Rgb& color : mode.colors)
        {
            color = scaleColor(color, level);
        }
    }
    else if((mode.colorMode == ColorMode::None || mode.colorMode == ColorMode::Random) && mode.hasBrightness)
    {
        mode.brightness = scaleBrightness(mode.brightness, mode.brightnessMin, level);
    }
}

void addModeSlots(const ModeState& mode, std::vector<Rgb>& slots)
{
    if(mode.colorMode == ColorMode::ModeSpecific)
    {
        slots.insert(slots.end(), mode.colors.begin(), mode.colors.end());
    }
}

std::vector<Rgb> colorSlots(const LightState& state)
{
    std::vector<Rgb> slots;
    addModeSlots(state.mode, slots);
    for(const ZoneState& zone : state.zones)
    {
        const ColorMode effective = (zone.mode.index >= 0) ? zone.mode.colorMode : state.mode.colorMode;
        if(effective == ColorMode::PerLed)
        {
            slots.insert(slots.end(), zone.leds.begin(), zone.leds.end());
        }
        if(zone.mode.index >= 0)
        {
            addModeSlots(zone.mode, slots);
        }
    }
    return slots;
}

void paintMode(ModeState& mode, Rgb accent)
{
    if(mode.colorMode == ColorMode::ModeSpecific)
    {
        for(Rgb& color : mode.colors)
        {
            color = accent;
        }
    }
}
}

Rgb scaleColor(Rgb color, int level)
{
    return makeRgb(scaleChannel(rgbRed(color), level), scaleChannel(rgbGreen(color), level), scaleChannel(rgbBlue(color), level));
}

unsigned int scaleBrightness(unsigned int brightness, unsigned int minimum, int level)
{
    const int delta = int(brightness) - int(minimum);
    const int scaled = (delta * level + (delta >= 0 ? 50 : -50)) / 100;
    return (unsigned int)(int(minimum) + scaled);
}

LightState renderAtLevel(const LightState& base, int level)
{
    LightState target = base;
    scaleMode(target.mode, level);
    for(ZoneState& zone : target.zones)
    {
        const ColorMode effective = (zone.mode.index >= 0) ? zone.mode.colorMode : target.mode.colorMode;
        if(effective == ColorMode::PerLed)
        {
            for(Rgb& led : zone.leds)
            {
                led = scaleColor(led, level);
            }
        }
        if(zone.mode.index >= 0)
        {
            scaleMode(zone.mode, level);
        }
    }
    return target;
}

LightState withAccent(const LightState& base, Rgb accent)
{
    LightState result = base;
    paintMode(result.mode, accent);
    for(ZoneState& zone : result.zones)
    {
        const ColorMode effective = (zone.mode.index >= 0) ? zone.mode.colorMode : result.mode.colorMode;
        if(effective == ColorMode::PerLed)
        {
            for(Rgb& led : zone.leds)
            {
                led = accent;
            }
        }
        if(zone.mode.index >= 0)
        {
            paintMode(zone.mode, accent);
        }
    }
    return result;
}

bool isUnlit(const LightState& state)
{
    const std::vector<Rgb> slots = colorSlots(state);
    return !slots.empty() && std::all_of(slots.begin(), slots.end(), [](Rgb color) { return color == 0; });
}

std::optional<Rgb> litColor(const LightState& state)
{
    std::vector<Rgb> lit;
    for(Rgb color : colorSlots(state))
    {
        if(color != 0)
        {
            lit.push_back(color);
        }
    }
    return blendColors(lit);
}

std::optional<Rgb> blendColors(const std::vector<Rgb>& colors)
{
    if(colors.empty())
    {
        return std::nullopt;
    }
    const unsigned int count = (unsigned int)colors.size();
    unsigned int red = 0;
    unsigned int green = 0;
    unsigned int blue = 0;
    unsigned int peaks = 0;
    for(Rgb color : colors)
    {
        red += rgbRed(color);
        green += rgbGreen(color);
        blue += rgbBlue(color);
        peaks += std::max({rgbRed(color), rgbGreen(color), rgbBlue(color)});
    }
    red = (red + count / 2) / count;
    green = (green + count / 2) / count;
    blue = (blue + count / 2) / count;
    const unsigned int peak = std::max({red, green, blue});
    if(peak == 0)
    {
        return makeRgb(0, 0, 0);
    }
    const unsigned int wanted = (peaks + count / 2) / count;
    const auto lift = [peak, wanted](unsigned int channel) { return std::min(255u, (channel * wanted + peak / 2) / peak); };
    return makeRgb(lift(red), lift(green), lift(blue));
}
