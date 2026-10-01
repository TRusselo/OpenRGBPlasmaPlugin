#pragma once

#include <map>
#include <string>

#include <nlohmann/json.hpp>

inline const char* const SettingsKey = "PlasmaIntegrationPlugin";

struct DeviceOptions
{
    bool dim = true;
    bool accent = true;
};

struct PluginSettings
{
    bool accentEnabled = false;
    std::map<std::string, DeviceOptions> devices;

    DeviceOptions optionsFor(const std::string& key) const;
    static PluginSettings fromJson(const nlohmann::json& json);
    nlohmann::json toJson() const;
};
