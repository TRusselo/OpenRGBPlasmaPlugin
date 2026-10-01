#include "PluginSettings.h"

namespace
{
void readBool(const nlohmann::json& object, const char* key, bool& value)
{
    if(object.contains(key) && object.at(key).is_boolean())
    {
        value = object.at(key).get<bool>();
    }
}
}

DeviceOptions PluginSettings::optionsFor(const std::string& key) const
{
    const auto found = devices.find(key);
    return found == devices.end() ? DeviceOptions() : found->second;
}

PluginSettings PluginSettings::fromJson(const nlohmann::json& json)
{
    PluginSettings settings;
    if(!json.is_object())
    {
        return settings;
    }

    readBool(json, "accent_enabled", settings.accentEnabled);

    if(json.contains("devices") && json.at("devices").is_object())
    {
        for(auto entry = json.at("devices").begin(); entry != json.at("devices").end(); ++entry)
        {
            if(!entry.value().is_object())
            {
                continue;
            }
            DeviceOptions options;
            readBool(entry.value(), "dim", options.dim);
            readBool(entry.value(), "accent", options.accent);
            settings.devices[entry.key()] = options;
        }
    }
    return settings;
}

nlohmann::json PluginSettings::toJson() const
{
    nlohmann::json json;
    json["accent_enabled"] = accentEnabled;
    json["devices"] = nlohmann::json::object();
    for(const auto& [key, options] : devices)
    {
        json["devices"][key] = {{"dim", options.dim}, {"accent", options.accent}};
    }
    return json;
}
