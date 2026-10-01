#include "TestStatusText.h"

#include <QTest>

#include "StatusText.h"

void TestStatusText::readyAndAccent()
{
    const QStringList lines = statusLines({SystemSetup::State::Ready, UledsBacklight::Status::Ready, true, false});
    QCOMPARE(lines, QStringList{QStringLiteral("Ready: Plasma's Keyboard Backlight slider controls these lights.")});
}

void TestStatusText::needsSetup()
{
    const QStringList lines = statusLines({SystemSetup::State::NeedsSetup, UledsBacklight::Status::NoPermission, true, false});
    QCOMPARE(lines, QStringList{QStringLiteral("Needs setup: click Set up to allow brightness control.")});
}

void TestStatusText::noKernelSupport()
{
    const QStringList lines = statusLines({SystemSetup::State::NoKernelSupport, UledsBacklight::Status::Missing, true, false});
    QCOMPARE(lines, QStringList{QStringLiteral("This kernel has no uleds: brightness control is off.")});
}

void TestStatusText::nameTaken()
{
    const QStringList lines = statusLines({SystemSetup::State::Ready, UledsBacklight::Status::NameTaken, true, false});
    QCOMPARE(lines, QStringList{QStringLiteral("Another OpenRGB instance owns the backlight.")});
}

void TestStatusText::accentUnavailableAndPowerDevil()
{
    const QStringList lines = statusLines({SystemSetup::State::Ready, UledsBacklight::Status::Ready, false, true});
    QCOMPARE(lines,
             (QStringList{QStringLiteral("Ready: Plasma's Keyboard Backlight slider controls these lights."),
                          QStringLiteral("PowerDevil has not picked up the backlight yet."),
                          QStringLiteral("Accent color unavailable (not running in Plasma).")}));
}
