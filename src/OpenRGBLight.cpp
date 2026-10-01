#include "OpenRGBLight.h"

#include "DeviceKey.h"
#include "RGBControllerInterface.h"

namespace
{
ModeState readDeviceMode(RGBControllerInterface* controller, int index)
{
    ModeState mode;
    if(index < 0 || index >= int(controller->GetModeCount()))
    {
        return mode;
    }
    const unsigned int modeIndex = (unsigned int)index;
    mode.index = index;
    mode.colorMode = ColorMode(controller->GetModeColorMode(modeIndex));
    mode.hasBrightness = (controller->GetModeFlags(modeIndex) & MODE_FLAG_HAS_BRIGHTNESS) != 0;
    mode.brightness = controller->GetModeBrightness(modeIndex);
    mode.brightnessMin = controller->GetModeBrightnessMin(modeIndex);
    mode.brightnessMax = controller->GetModeBrightnessMax(modeIndex);
    if(mode.colorMode == ColorMode::ModeSpecific)
    {
        for(unsigned int i = 0; i < controller->GetModeColorsCount(modeIndex); i++)
        {
            mode.colors.push_back(controller->GetModeColor(modeIndex, i));
        }
    }
    return mode;
}

ModeState readZoneMode(RGBControllerInterface* controller, unsigned int zone, int index)
{
    ModeState mode;
    if(index < 0 || index >= int(controller->GetZoneModeCount(zone)))
    {
        return mode;
    }
    const unsigned int modeIndex = (unsigned int)index;
    mode.index = index;
    mode.colorMode = ColorMode(controller->GetZoneModeColorMode(zone, modeIndex));
    mode.hasBrightness = (controller->GetZoneModeFlags(zone, modeIndex) & MODE_FLAG_HAS_BRIGHTNESS) != 0;
    mode.brightness = controller->GetZoneModeBrightness(zone, modeIndex);
    mode.brightnessMin = controller->GetZoneModeBrightnessMin(zone, modeIndex);
    mode.brightnessMax = controller->GetZoneModeBrightnessMax(zone, modeIndex);
    if(mode.colorMode == ColorMode::ModeSpecific)
    {
        for(unsigned int i = 0; i < controller->GetZoneModeColorsCount(zone, modeIndex); i++)
        {
            mode.colors.push_back(controller->GetZoneModeColor(zone, modeIndex, i));
        }
    }
    return mode;
}
}

OpenRGBLight::OpenRGBLight(RGBControllerInterface* controller)
    : rgb(controller)
    , keyValue(deviceKey(controller->GetName(), controller->GetSerial(), controller->GetLocation()))
    , nameValue(controller->GetName())
{
}

RGBControllerInterface* OpenRGBLight::controller() const
{
    return rgb;
}

std::string OpenRGBLight::key() const
{
    return keyValue;
}

std::string OpenRGBLight::name() const
{
    return nameValue;
}

LightState OpenRGBLight::read() const
{
    LightState state;
    state.mode = readDeviceMode(rgb, rgb->GetActiveMode());
    for(unsigned int zone = 0; zone < rgb->GetZoneCount(); zone++)
    {
        ZoneState zoneState;
        zoneState.mode = readZoneMode(rgb, zone, rgb->GetZoneActiveMode(zone));
        const unsigned int start = rgb->GetZoneStartIndex(zone);
        const unsigned int count = rgb->GetZoneLEDsCount(zone);
        for(unsigned int led = 0; led < count; led++)
        {
            zoneState.leds.push_back(rgb->GetColor(start + led));
        }
        state.zones.push_back(zoneState);
    }
    return state;
}

void OpenRGBLight::apply(const LightState& target)
{
    const LightState current = read();
    if(target.mode.index != current.mode.index || target.zones.size() != current.zones.size())
    {
        return;
    }

    if(target.mode.index >= 0 && target.mode != current.mode)
    {
        const unsigned int modeIndex = (unsigned int)target.mode.index;
        for(std::size_t i = 0; i < target.mode.colors.size(); i++)
        {
            rgb->SetModeColor(modeIndex, (unsigned int)i, target.mode.colors[i]);
        }
        if(target.mode.hasBrightness)
        {
            rgb->SetModeBrightness(modeIndex, target.mode.brightness);
        }
        rgb->UpdateMode();
    }

    bool ledsChanged = false;
    for(unsigned int zone = 0; zone < target.zones.size(); zone++)
    {
        const ZoneState& want = target.zones[zone];
        const ZoneState& have = current.zones[zone];
        if(want.mode.index >= 0 && want.mode.index == have.mode.index && want.mode != have.mode)
        {
            const unsigned int modeIndex = (unsigned int)want.mode.index;
            for(std::size_t i = 0; i < want.mode.colors.size(); i++)
            {
                rgb->SetZoneModeColor(zone, modeIndex, (unsigned int)i, want.mode.colors[i]);
            }
            if(want.mode.hasBrightness)
            {
                rgb->SetZoneModeBrightness(zone, modeIndex, want.mode.brightness);
            }
            rgb->UpdateZoneMode(int(zone));
        }
        if(want.leds != have.leds && want.leds.size() == have.leds.size())
        {
            const unsigned int start = rgb->GetZoneStartIndex(zone);
            for(std::size_t led = 0; led < want.leds.size(); led++)
            {
                rgb->SetColor(start + (unsigned int)led, want.leds[led]);
            }
            ledsChanged = true;
        }
    }
    if(ledsChanged)
    {
        rgb->UpdateLEDs();
    }
}
