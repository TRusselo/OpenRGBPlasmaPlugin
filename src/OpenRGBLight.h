#pragma once

#include <string>

#include "Light.h"

class RGBControllerInterface;

class OpenRGBLight : public Light
{
public:
    explicit OpenRGBLight(RGBControllerInterface* controller);

    RGBControllerInterface* controller() const;

    std::string key() const override;
    std::string name() const override;
    LightState read() const override;
    void apply(const LightState& target) override;

private:
    RGBControllerInterface* rgb;
    std::string keyValue;
    std::string nameValue;
};
