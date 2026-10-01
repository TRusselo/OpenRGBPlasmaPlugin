#include "LightingEngine.h"

#include <algorithm>

#include "Dimming.h"

namespace
{
constexpr std::size_t RecentLimit = 8;
}

LightingEngine::LightingEngine(WriteRequest writeRequest)
    : requestWrite(std::move(writeRequest))
{
}

void LightingEngine::setLights(const std::vector<std::shared_ptr<Light>>& lights)
{
    std::map<std::string, Entry> updated;
    for(const std::shared_ptr<Light>& light : lights)
    {
        const std::string key = light->key();
        const auto existing = entries.find(key);
        if(existing != entries.end() && sameShape(existing->second.base, light->read()))
        {
            Entry entry = existing->second;
            entry.light = light;
            updated[key] = entry;
            continue;
        }
        updated[key] = newEntry(key, light);
    }
    entries = std::move(updated);
    fillUnlit();
    for(auto& [key, entry] : entries)
    {
        if(!entry.writing)
        {
            render(key, entry);
        }
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
            entry.unlit = false;
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
    fillUnlit();
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
        entry.unlit = false;
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
    if(isOwnState(entry, state))
    {
        return;
    }
    adoptOutsideState(entry, state);
}

void LightingEngine::onWriteFinished(const std::string& key, std::uint64_t sequence)
{
    const auto found = entries.find(key);
    if(found == entries.end())
    {
        return;
    }
    Entry& entry = found->second;
    if(sequence != entry.sequence)
    {
        return;
    }
    entry.writing = false;
    const LightState state = entry.light->read();
    if(!isOwnState(entry, state))
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
    entry.sequence = ++nextSequence;
    entry.recent.push_back(target);
    if(entry.recent.size() > RecentLimit)
    {
        entry.recent.pop_front();
    }
    requestWrite(key, target, entry.sequence);
}

LightingEngine::Entry LightingEngine::newEntry(const std::string& key, const std::shared_ptr<Light>& light) const
{
    const DeviceOptions options = settings.optionsFor(key);
    Entry entry;
    entry.light = light;
    entry.base = light->read();
    if(accent && options.accent)
    {
        entry.base = withAccent(entry.base, *accent);
    }
    entry.follows = options.dim;
    entry.unlit = isUnlit(entry.base);
    return entry;
}

void LightingEngine::fillUnlit()
{
    std::vector<Rgb> colors;
    for(const auto& [key, entry] : entries)
    {
        const std::optional<Rgb> color = entry.unlit ? std::nullopt : litColor(entry.base);
        if(color)
        {
            colors.push_back(*color);
        }
    }
    const std::optional<Rgb> fill = blendColors(colors);
    if(!fill)
    {
        return;
    }
    for(auto& [key, entry] : entries)
    {
        if(entry.unlit && settings.optionsFor(key).dim)
        {
            entry.base = withAccent(entry.base, *fill);
            entry.unlit = false;
            entry.follows = true;
        }
    }
}

bool LightingEngine::isOwnState(const Entry& entry, const LightState& state)
{
    if(entry.expected && state == *entry.expected)
    {
        return true;
    }
    return std::find(entry.recent.begin(), entry.recent.end(), state) != entry.recent.end();
}

bool LightingEngine::sameShape(const LightState& a, const LightState& b)
{
    if(a.zones.size() != b.zones.size())
    {
        return false;
    }
    for(std::size_t zone = 0; zone < a.zones.size(); zone++)
    {
        if(a.zones[zone].leds.size() != b.zones[zone].leds.size())
        {
            return false;
        }
    }
    return true;
}

void LightingEngine::adoptOutsideState(Entry& entry, const LightState& state)
{
    entry.base = state;
    entry.unlit = false;
    entry.follows = false;
    entry.expected.reset();
}
