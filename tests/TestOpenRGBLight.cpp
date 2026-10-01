#include "TestOpenRGBLight.h"

#include <QTest>

#include "FakeController.h"
#include "LightBuilders.h"
#include "OpenRGBLight.h"

namespace
{
FakeController::FakeMode fakeDirect()
{
    FakeController::FakeMode mode;
    mode.colorMode = MODE_COLORS_PER_LED;
    return mode;
}

FakeController::FakeMode fakeStatic(RGBColor color)
{
    FakeController::FakeMode mode;
    mode.colorMode = MODE_COLORS_MODE_SPECIFIC;
    mode.colors = {color};
    return mode;
}

FakeController::FakeZone fakeZone(unsigned int start, unsigned int count)
{
    FakeController::FakeZone zone;
    zone.start = start;
    zone.ledsCount = count;
    zone.ledsInZone = count;
    return zone;
}

void makeTwoZoneDirect(FakeController& fake)
{
    fake.modes = {fakeDirect(), fakeStatic(makeRgb(255, 0, 0))};
    fake.activeMode = 0;
    fake.zones = {fakeZone(0, 2), fakeZone(2, 1)};
    fake.colors = {makeRgb(10, 20, 30), makeRgb(40, 50, 60), makeRgb(70, 80, 90)};
}
}

void TestOpenRGBLight::keyUsesNameAndSerial()
{
    FakeController fake;
    OpenRGBLight light(&fake);
    QCOMPARE(QString::fromStdString(light.key()), QStringLiteral("Fake Device|SERIAL1"));
}

void TestOpenRGBLight::readsDeviceModeAndZoneLeds()
{
    FakeController fake;
    makeTwoZoneDirect(fake);
    OpenRGBLight light(&fake);

    LightState expected;
    expected.mode = perLedMode(0);
    ZoneState first;
    first.leds = {makeRgb(10, 20, 30), makeRgb(40, 50, 60)};
    ZoneState second;
    second.leds = {makeRgb(70, 80, 90)};
    expected.zones = {first, second};

    QCOMPARE(light.read(), expected);
}

void TestOpenRGBLight::readsZoneModeWhenSet()
{
    FakeController fake;
    makeTwoZoneDirect(fake);
    fake.zones[1].modes = {fakeStatic(makeRgb(0, 0, 200))};
    fake.zones[1].activeMode = 0;
    OpenRGBLight light(&fake);

    const LightState state = light.read();
    QCOMPARE(state.zones[0].mode.index, -1);
    QCOMPARE(state.zones[1].mode, staticMode(0, makeRgb(0, 0, 200)));
}

void TestOpenRGBLight::applyWritesOnlyChangedLeds()
{
    FakeController fake;
    makeTwoZoneDirect(fake);
    OpenRGBLight light(&fake);
    LightState target = light.read();
    target.zones[1].leds[0] = makeRgb(1, 2, 3);

    light.apply(target);

    QCOMPARE(fake.colors[2], makeRgb(1, 2, 3));
    QCOMPARE(fake.colors[0], makeRgb(10, 20, 30));
    QCOMPARE(fake.updateLEDsCalls, 1);
    QCOMPARE(fake.updateModeCalls, 0);
    QCOMPARE(fake.updateZoneModeCalls, 0);
}

void TestOpenRGBLight::applyUpdatesModeColors()
{
    FakeController fake;
    makeTwoZoneDirect(fake);
    fake.activeMode = 1;
    OpenRGBLight light(&fake);
    LightState target = light.read();
    target.mode.colors[0] = makeRgb(9, 9, 9);

    light.apply(target);

    QCOMPARE(fake.modes[1].colors[0], makeRgb(9, 9, 9));
    QCOMPARE(fake.updateModeCalls, 1);
    QCOMPARE(fake.updateLEDsCalls, 0);
}

void TestOpenRGBLight::applySkipsWhenModeChanged()
{
    FakeController fake;
    makeTwoZoneDirect(fake);
    OpenRGBLight light(&fake);
    LightState target = light.read();
    target.zones[0].leds[0] = makeRgb(1, 1, 1);
    fake.activeMode = 1;

    light.apply(target);

    QCOMPARE(fake.colors[0], makeRgb(10, 20, 30));
    QCOMPARE(fake.updateLEDsCalls, 0);
    QCOMPARE(fake.updateModeCalls, 0);
}

void TestOpenRGBLight::effectsOnlyZoneReadsOnlyItsColors()
{
    FakeController fake;
    fake.modes = {fakeDirect()};
    FakeController::FakeZone effectsOnly = fakeZone(0, 60);
    effectsOnly.ledsInZone = 1;
    fake.zones = {effectsOnly, fakeZone(1, 1)};
    fake.colors = {makeRgb(10, 20, 30), makeRgb(40, 50, 60)};
    OpenRGBLight light(&fake);

    const LightState state = light.read();

    QCOMPARE(int(state.zones[0].leds.size()), 1);
    QCOMPARE(state.zones[0].leds[0], makeRgb(10, 20, 30));
    QCOMPARE(state.zones[1].leds[0], makeRgb(40, 50, 60));
}
