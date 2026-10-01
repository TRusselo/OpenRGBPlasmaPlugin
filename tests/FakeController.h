#pragma once

#include <string>
#include <vector>

#include "StubController.h"

class FakeController : public StubController
{
public:
    struct FakeMode
    {
        unsigned int flags = 0;
        unsigned int colorMode = MODE_COLORS_NONE;
        unsigned int brightness = 0;
        unsigned int brightnessMin = 0;
        unsigned int brightnessMax = 0;
        std::vector<RGBColor> colors;
    };

    struct FakeZone
    {
        unsigned int start = 0;
        unsigned int ledsCount = 0;
        unsigned int ledsInZone = 0;
        std::vector<FakeMode> modes;
        int activeMode = -1;
    };

    std::string name = "Fake Device";
    std::string serial = "SERIAL1";
    std::string location = "HID: /dev/hidraw0";
    std::vector<FakeMode> modes;
    int activeMode = 0;
    std::vector<FakeZone> zones;
    std::vector<RGBColor> colors;
    int updateLEDsCalls = 0;
    int updateModeCalls = 0;
    int updateZoneModeCalls = 0;

    std::string GetName() override { return name; }
    std::string GetSerial() override { return serial; }
    std::string GetLocation() override { return location; }
    std::string GetDisplayName() override { return name; }

    unsigned int GetModeCount() override { return (unsigned int)modes.size(); }
    int GetActiveMode() override { return activeMode; }
    unsigned int GetModeColorMode(unsigned int mode) override { return modes.at(mode).colorMode; }
    unsigned int GetModeFlags(unsigned int mode) override { return modes.at(mode).flags; }
    unsigned int GetModeBrightness(unsigned int mode) override { return modes.at(mode).brightness; }
    unsigned int GetModeBrightnessMin(unsigned int mode) override { return modes.at(mode).brightnessMin; }
    unsigned int GetModeBrightnessMax(unsigned int mode) override { return modes.at(mode).brightnessMax; }
    unsigned int GetModeColorsCount(unsigned int mode) override { return (unsigned int)modes.at(mode).colors.size(); }
    RGBColor GetModeColor(unsigned int mode, unsigned int color_index) override { return modes.at(mode).colors.at(color_index); }
    void SetModeColor(unsigned int mode, unsigned int color_index, RGBColor color) override { modes.at(mode).colors.at(color_index) = color; }
    void SetModeBrightness(unsigned int mode, unsigned int brightness) override { modes.at(mode).brightness = brightness; }
    void UpdateMode() override { updateModeCalls++; }
    void SetActiveMode(int mode) override
    {
        activeMode = mode;
        updateModeCalls++;
    }
    void SetZoneActiveMode(unsigned int zone, int mode) override
    {
        zones.at(zone).activeMode = mode;
        updateZoneModeCalls++;
    }

    unsigned int GetZoneCount() override { return (unsigned int)zones.size(); }
    int GetZoneActiveMode(unsigned int zone) override { return zones.at(zone).activeMode; }
    unsigned int GetZoneModeCount(unsigned int zone) override { return (unsigned int)zones.at(zone).modes.size(); }
    unsigned int GetZoneModeColorMode(unsigned int zone, unsigned int mode) override { return zones.at(zone).modes.at(mode).colorMode; }
    unsigned int GetZoneModeFlags(unsigned int zone, unsigned int mode) override { return zones.at(zone).modes.at(mode).flags; }
    unsigned int GetZoneModeBrightness(unsigned int zone, unsigned int mode) override { return zones.at(zone).modes.at(mode).brightness; }
    unsigned int GetZoneModeBrightnessMin(unsigned int zone, unsigned int mode) override { return zones.at(zone).modes.at(mode).brightnessMin; }
    unsigned int GetZoneModeBrightnessMax(unsigned int zone, unsigned int mode) override { return zones.at(zone).modes.at(mode).brightnessMax; }
    unsigned int GetZoneModeColorsCount(unsigned int zone, unsigned int mode) override { return (unsigned int)zones.at(zone).modes.at(mode).colors.size(); }
    RGBColor GetZoneModeColor(unsigned int zone, unsigned int mode, unsigned int color_index) override { return zones.at(zone).modes.at(mode).colors.at(color_index); }
    void SetZoneModeColor(unsigned int zone, unsigned int mode, unsigned int color_index, RGBColor color) override { zones.at(zone).modes.at(mode).colors.at(color_index) = color; }
    void SetZoneModeBrightness(unsigned int zone, unsigned int mode, unsigned int brightness) override { zones.at(zone).modes.at(mode).brightness = brightness; }
    void UpdateZoneMode(int) override { updateZoneModeCalls++; }

    unsigned int GetZoneStartIndex(unsigned int zone) override { return zones.at(zone).start; }
    unsigned int GetZoneLEDsCount(unsigned int zone) override { return zones.at(zone).ledsCount; }
    unsigned int GetLEDsInZone(unsigned int zone) override { return zones.at(zone).ledsInZone; }
    RGBColor GetColor(unsigned int led) override { return led < colors.size() ? colors[led] : 0; }
    void SetColor(unsigned int led, RGBColor color) override
    {
        if(led < colors.size())
        {
            colors[led] = color;
        }
    }
    void UpdateLEDs() override { updateLEDsCalls++; }
};
