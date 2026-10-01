#include "Dimming.h"

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
