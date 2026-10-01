#pragma once

#include <cstring>
#include <utility>
#include <vector>

#include <QString>

#include "LightModel.h"

inline ModeState perLedMode(int index)
{
    ModeState mode;
    mode.index = index;
    mode.colorMode = ColorMode::PerLed;
    return mode;
}

inline ModeState staticMode(int index, Rgb color)
{
    ModeState mode;
    mode.index = index;
    mode.colorMode = ColorMode::ModeSpecific;
    mode.colors = {color};
    return mode;
}

inline ModeState offMode(int index)
{
    ModeState mode;
    mode.index = index;
    mode.colorMode = ColorMode::None;
    return mode;
}

inline ModeState brightnessMode(int index, unsigned int brightness, unsigned int minimum, unsigned int maximum)
{
    ModeState mode;
    mode.index = index;
    mode.colorMode = ColorMode::None;
    mode.hasBrightness = true;
    mode.brightness = brightness;
    mode.brightnessMin = minimum;
    mode.brightnessMax = maximum;
    return mode;
}

inline LightState directLight(std::vector<Rgb> leds)
{
    LightState state;
    state.mode = perLedMode(0);
    ZoneState zone;
    zone.leds = std::move(leds);
    state.zones.push_back(zone);
    return state;
}

inline LightState lightWithMode(const ModeState& mode, std::vector<Rgb> leds)
{
    LightState state;
    state.mode = mode;
    ZoneState zone;
    zone.leds = std::move(leds);
    state.zones.push_back(zone);
    return state;
}

inline char* toString(const LightState& state)
{
    QString text = QStringLiteral("mode %1 colorMode %2 brightness %3").arg(state.mode.index).arg(int(state.mode.colorMode)).arg(state.mode.brightness);
    for(Rgb color : state.mode.colors)
    {
        text += QStringLiteral(" m%1").arg(color, 6, 16, QLatin1Char('0'));
    }
    for(const ZoneState& zone : state.zones)
    {
        text += QStringLiteral(" | zone mode %1").arg(zone.mode.index);
        for(Rgb color : zone.mode.colors)
        {
            text += QStringLiteral(" m%1").arg(color, 6, 16, QLatin1Char('0'));
        }
        for(Rgb color : zone.leds)
        {
            text += QStringLiteral(" %1").arg(color, 6, 16, QLatin1Char('0'));
        }
    }
    return qstrdup(text.toUtf8().constData());
}
