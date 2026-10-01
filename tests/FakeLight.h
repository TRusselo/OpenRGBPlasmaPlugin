#pragma once

#include <string>
#include <utility>

#include "Light.h"

class FakeLight : public Light
{
public:
    FakeLight(std::string lightKey, LightState initial)
        : keyValue(std::move(lightKey))
        , state(std::move(initial))
    {
    }

    std::string key() const override
    {
        return keyValue;
    }

    std::string name() const override
    {
        return keyValue;
    }

    LightState read() const override
    {
        return state;
    }

    void apply(const LightState& target, bool restoreMode) override
    {
        state = target;
        lastRestoreMode = restoreMode;
        applyCount++;
    }

    std::string keyValue;
    LightState state;
    int applyCount = 0;
    bool lastRestoreMode = false;
};
