#include "TestUPowerLevelSync.h"

#include <QTest>

#include "UPowerLevelSync.h"

namespace
{
const QString Own = QStringLiteral("/org/freedesktop/UPower/KbdBacklight/openrgbookbd_backlight");
const QString Laptop = QStringLiteral("/org/freedesktop/UPower/KbdBacklight/tpacpiookbd_backlight");
}

void TestUPowerLevelSync::ownPathMatchesNativePathName()
{
    QMap<QString, QString> nativePaths;
    nativePaths[Laptop] = QStringLiteral("/sys/devices/platform/thinkpad_acpi/leds/tpacpi::kbd_backlight");
    nativePaths[QStringLiteral("/x")] = QStringLiteral("/sys/devices/virtual/misc/uleds/xopenrgb::kbd_backlight");
    nativePaths[Own] = QStringLiteral("/sys/devices/virtual/misc/uleds/openrgb::kbd_backlight");

    QCOMPARE(UPowerLevelSync::ownPath(nativePaths, QStringLiteral("openrgb::kbd_backlight")), Own);
    nativePaths.remove(Own);
    QCOMPARE(UPowerLevelSync::ownPath(nativePaths, QStringLiteral("openrgb::kbd_backlight")), QString());
}

void TestUPowerLevelSync::onlyOwnBacklightSetsSharedLevelToMaximum()
{
    const auto write = UPowerLevelSync::plan({Own}, Own, 0, 100, 100);
    QVERIFY(write.has_value());
    QCOMPARE(write->path, UPowerLevelSync::CompositePath);
    QCOMPARE(write->value, 100);
}

void TestUPowerLevelSync::otherBacklightsShareTheirLevel()
{
    const auto write = UPowerLevelSync::plan({Laptop, Own}, Own, 2, 3, 100);
    QVERIFY(write.has_value());
    QCOMPARE(write->path, Own);
    QCOMPARE(write->value, 67);
}

void TestUPowerLevelSync::ownNotExportedYetWritesNothing()
{
    QVERIFY(!UPowerLevelSync::plan({Laptop}, QString(), 2, 3, 100).has_value());
    QVERIFY(!UPowerLevelSync::plan({}, QString(), 0, 0, 100).has_value());
}
