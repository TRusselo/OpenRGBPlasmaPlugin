#include "StatusText.h"

QStringList statusLines(const StatusInput& input)
{
    QStringList lines;
    if(input.setup == SystemSetup::State::NoKernelSupport)
    {
        lines << QStringLiteral("This kernel has no uleds: brightness control is off.");
    }
    else if(input.backlight == UledsBacklight::Status::Ready)
    {
        lines << QStringLiteral("Ready: Plasma's Keyboard Backlight slider controls these lights.");
    }
    else if(input.backlight == UledsBacklight::Status::NameTaken)
    {
        lines << QStringLiteral("Another OpenRGB instance owns the backlight.");
    }
    else if(input.setup == SystemSetup::State::NeedsSetup || input.backlight == UledsBacklight::Status::NoPermission
            || input.backlight == UledsBacklight::Status::Missing)
    {
        lines << QStringLiteral("Needs setup: click Set up to allow brightness control.");
    }
    else
    {
        lines << QStringLiteral("Could not create the backlight (see the OpenRGB log).");
    }

    if(input.powerDevilNeedsRestart)
    {
        lines << QStringLiteral("PowerDevil has not picked up the backlight yet.");
    }
    if(!input.accentAvailable)
    {
        lines << QStringLiteral("Accent color unavailable (not running in Plasma).");
    }
    return lines;
}
