#include "LightingEngine.h"

#include <algorithm>

#include "Dimming.h"

LightingEngine::LightingEngine(WriteRequest writeRequest)
    : requestWrite(std::move(writeRequest))
{
}

void LightingEngine::setLights(const std::vector<std::shared_ptr<Light>>& lights)
{
    std::map<std::string, Entry> updated;
    std::vector<std::string> added;
    for(const std::shared_ptr<Light>& light : lights)
    {
        const std::string key = light->key();
        const auto existing = entries.find(key);
        if(existing != entries.end())
        {
            Entry entry = existing->second;
            entry.light = light;
            updated[key] = entry;
            continue;
        }
        Entry entry;
        entry.light = light;
        entry.base = light->read();
        entry.follows = settings.optionsFor(key).dim;
        updated[key] = entry;
        added.push_back(key);
    }
    entries = std::move(updated);
    for(const std::string& key : added)
    {
        render(key, entries[key]);
    }
}

void LightingEngine::setSettings(const PluginSettings& newSettings)
{
    const PluginSettings old = settings;
    settings = newSettings;
    for(auto& [key, entry] : entries)
    {
        const DeviceOptions before = old.optionsFor(key);
        const DeviceOptions after = settings.optionsFor(key);
        bool changed = false;
        if(after.accent && !before.accent && accent)
        {
            entry.base = withAccent(entry.base, *accent);
            changed = true;
        }
        if(after.dim != before.dim || changed)
        {
            entry.follows = after.dim;
            changed = true;
        }
        if(changed)
        {
            render(key, entry);
        }
    }
}

void LightingEngine::setLevel(int level)
{
    currentLevel = std::clamp(level, 0, 100);
    for(auto& [key, entry] : entries)
    {
        if(settings.optionsFor(key).dim)
        {
            entry.follows = true;
            render(key, entry);
        }
    }
}

void LightingEngine::setAccent(std::optional<Rgb> newAccent)
{
    accent = newAccent;
    if(!accent)
    {
        return;
    }
    for(auto& [key, entry] : entries)
    {
        const DeviceOptions options = settings.optionsFor(key);
        if(!options.accent)
        {
            continue;
        }
        entry.base = withAccent(entry.base, *accent);
        entry.follows = options.dim;
        render(key, entry);
    }
}

void LightingEngine::onLightChanged(const std::string& key)
{
    const auto found = entries.find(key);
    if(found == entries.end())
    {
        return;
    }
    Entry& entry = found->second;
    if(entry.writing)
    {
        return;
    }
    const LightState state = entry.light->read();
    if(entry.expected && state == *entry.expected)
    {
        return;
    }
    adoptOutsideState(entry, state);
}

void LightingEngine::onWriteFinished(const std::string& key, const LightState& applied)
{
    const auto found = entries.find(key);
    if(found == entries.end())
    {
        return;
    }
    Entry& entry = found->second;
    if(!entry.expected || applied != *entry.expected)
    {
        return;
    }
    entry.writing = false;
    const LightState state = entry.light->read();
    if(state != *entry.expected)
    {
        adoptOutsideState(entry, state);
    }
}

int LightingEngine::level() const
{
    return currentLevel;
}

bool LightingEngine::follows(const std::string& key) const
{
    const auto found = entries.find(key);
    return found != entries.end() && found->second.follows;
}

std::optional<LightState> LightingEngine::base(const std::string& key) const
{
    const auto found = entries.find(key);
    if(found == entries.end())
    {
        return std::nullopt;
    }
    return found->second.base;
}

void LightingEngine::render(const std::string& key, Entry& entry)
{
    const LightState target = entry.follows ? renderAtLevel(entry.base, currentLevel) : entry.base;
    entry.expected = target;
    if(!entry.writing && entry.light->read() == target)
    {
        return;
    }
    entry.writing = true;
    requestWrite(key, target);
}

void LightingEngine::adoptOutsideState(Entry& entry, const LightState& state)
{
    entry.base = state;
    entry.follows = false;
    entry.expected.reset();
}
