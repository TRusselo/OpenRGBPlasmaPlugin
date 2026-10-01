#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Light.h"
#include "PluginSettings.h"

class LightingEngine
{
public:
    using WriteRequest = std::function<void(const std::string& key, const LightState& target)>;

    explicit LightingEngine(WriteRequest writeRequest);

    void setLights(const std::vector<std::shared_ptr<Light>>& lights);
    void setSettings(const PluginSettings& settings);
    void setLevel(int level);
    void setAccent(std::optional<Rgb> accent);
    void onLightChanged(const std::string& key);
    void onWriteFinished(const std::string& key, const LightState& applied);

    int level() const;
    bool follows(const std::string& key) const;
    std::optional<LightState> base(const std::string& key) const;

private:
    struct Entry
    {
        std::shared_ptr<Light> light;
        LightState base;
        bool follows = false;
        bool writing = false;
        std::optional<LightState> expected;
    };

    void render(const std::string& key, Entry& entry);
    static void adoptOutsideState(Entry& entry, const LightState& state);

    WriteRequest requestWrite;
    std::map<std::string, Entry> entries;
    PluginSettings settings;
    std::optional<Rgb> accent;
    int currentLevel = 100;
};
