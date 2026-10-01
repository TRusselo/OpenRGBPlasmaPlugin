#include "TestDimming.h"

#include <QTest>

#include "Dimming.h"
#include "LightBuilders.h"

void TestDimming::scaleColorKeepsColorAtFullLevel()
{
    QCOMPARE(scaleColor(makeRgb(255, 128, 1), 100), makeRgb(255, 128, 1));
}

void TestDimming::scaleColorIsBlackAtZero()
{
    QCOMPARE(scaleColor(makeRgb(255, 128, 1), 0), makeRgb(0, 0, 0));
}

void TestDimming::scaleColorRoundsEachChannel()
{
    QCOMPARE(scaleColor(makeRgb(255, 128, 1), 50), makeRgb(128, 64, 1));
    QCOMPARE(scaleColor(makeRgb(255, 0, 0), 40), makeRgb(102, 0, 0));
}

void TestDimming::scaleBrightnessMovesTowardsMinimum()
{
    QCOMPARE(scaleBrightness(80, 10, 50), 45u);
    QCOMPARE(scaleBrightness(80, 10, 0), 10u);
    QCOMPARE(scaleBrightness(80, 10, 100), 80u);
}

void TestDimming::scaleBrightnessHandlesInvertedRange()
{
    QCOMPARE(scaleBrightness(20, 100, 50), 60u);
    QCOMPARE(scaleBrightness(20, 100, 0), 100u);
}

void TestDimming::renderScalesPerLedColors()
{
    const LightState base = directLight({makeRgb(200, 100, 50), makeRgb(10, 20, 30)});
    QCOMPARE(renderAtLevel(base, 50), directLight({makeRgb(100, 50, 25), makeRgb(5, 10, 15)}));
}

void TestDimming::renderScalesModeSpecificColors()
{
    const LightState base = lightWithMode(staticMode(1, makeRgb(255, 0, 0)), {makeRgb(9, 9, 9)});
    QCOMPARE(renderAtLevel(base, 40), lightWithMode(staticMode(1, makeRgb(102, 0, 0)), {makeRgb(9, 9, 9)}));
}

void TestDimming::renderScalesBrightnessOfColorlessModes()
{
    const LightState base = lightWithMode(brightnessMode(2, 80, 10, 100), {});
    QCOMPARE(renderAtLevel(base, 50), lightWithMode(brightnessMode(2, 45, 10, 100), {}));
}

void TestDimming::renderLeavesOffModeUnchanged()
{
    const LightState base = lightWithMode(offMode(3), {makeRgb(50, 60, 70)});
    QCOMPARE(renderAtLevel(base, 0), base);
}

void TestDimming::renderUsesZoneModeWhenSet()
{
    LightState base;
    base.mode = perLedMode(0);
    ZoneState own;
    own.mode = staticMode(4, makeRgb(0, 0, 200));
    own.leds = {makeRgb(50, 50, 50)};
    ZoneState follower;
    follower.leds = {makeRgb(100, 100, 100)};
    base.zones = {own, follower};

    LightState expected = base;
    expected.zones[0].mode = staticMode(4, makeRgb(0, 0, 100));
    expected.zones[1].leds = {makeRgb(50, 50, 50)};

    QCOMPARE(renderAtLevel(base, 50), expected);
}

void TestDimming::renderHandlesEmptyLight()
{
    const LightState empty;
    QCOMPARE(renderAtLevel(empty, 30), empty);
    QCOMPARE(withAccent(empty, makeRgb(1, 2, 3)), empty);
}

void TestDimming::accentPaintsPerLedAndModeColors()
{
    const Rgb accent = makeRgb(61, 174, 233);
    QCOMPARE(withAccent(directLight({makeRgb(1, 1, 1), makeRgb(2, 2, 2)}), accent), directLight({accent, accent}));
    QCOMPARE(withAccent(lightWithMode(staticMode(1, makeRgb(255, 0, 0)), {makeRgb(9, 9, 9)}), accent),
             lightWithMode(staticMode(1, accent), {makeRgb(9, 9, 9)}));
}

void TestDimming::accentLeavesColorlessModesUnchanged()
{
    const LightState off = lightWithMode(offMode(3), {makeRgb(50, 60, 70)});
    QCOMPARE(withAccent(off, makeRgb(61, 174, 233)), off);
}
