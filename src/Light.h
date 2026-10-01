#pragma once

#include <string>

#include "LightModel.h"

class Light
{
public:
    virtual ~Light() = default;
    virtual std::string key() const = 0;
    virtual std::string name() const = 0;
    virtual LightState read() const = 0;
    virtual void apply(const LightState& target) = 0;
};
