#include "TestDeviceKey.h"

#include <QTest>

#include "DeviceKey.h"

void TestDeviceKey::usesSerialWhenPresent()
{
    QCOMPARE(QString::fromStdString(deviceKey("Razer Goliathus", "PM1234", "HID: /dev/hidraw7")), QStringLiteral("Razer Goliathus|PM1234"));
}

void TestDeviceKey::trimsSerial()
{
    QCOMPARE(QString::fromStdString(deviceKey("Razer Goliathus", "  PM1234 \n", "HID: /dev/hidraw7")), QStringLiteral("Razer Goliathus|PM1234"));
}

void TestDeviceKey::fallsBackToLocationForNone()
{
    QCOMPARE(QString::fromStdString(deviceKey("Fan", "none", "I2C: /dev/i2c-3, 0x60")), QStringLiteral("Fan|I2C: /dev/i2c-3, 0x60"));
    QCOMPARE(QString::fromStdString(deviceKey("Fan", "NONE", "I2C: /dev/i2c-3, 0x60")), QStringLiteral("Fan|I2C: /dev/i2c-3, 0x60"));
}

void TestDeviceKey::fallsBackToLocationForBlankSerial()
{
    QCOMPARE(QString::fromStdString(deviceKey("Fan", "", "HID: /dev/hidraw2")), QStringLiteral("Fan|HID: /dev/hidraw2"));
    QCOMPARE(QString::fromStdString(deviceKey("Fan", "   ", "HID: /dev/hidraw2")), QStringLiteral("Fan|HID: /dev/hidraw2"));
}
