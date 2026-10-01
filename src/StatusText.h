#pragma once

#include <QStringList>

#include "SystemSetup.h"
#include "UledsBacklight.h"

struct StatusInput
{
    SystemSetup::State setup = SystemSetup::State::NeedsSetup;
    UledsBacklight::Status backlight = UledsBacklight::Status::Closed;
    bool accentAvailable = false;
    bool powerDevilNeedsRestart = false;
};

QStringList statusLines(const StatusInput& input);
