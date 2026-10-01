#pragma once

#include <QObject>

class TestDimming : public QObject
{
    Q_OBJECT

private slots:
    void scaleColorKeepsColorAtFullLevel();
    void scaleColorIsBlackAtZero();
    void scaleColorRoundsEachChannel();
    void scaleBrightnessMovesTowardsMinimum();
    void scaleBrightnessHandlesInvertedRange();
    void renderScalesPerLedColors();
    void renderScalesModeSpecificColors();
    void renderScalesBrightnessOfColorlessModes();
    void renderLeavesOffModeUnchanged();
    void renderUsesZoneModeWhenSet();
    void renderHandlesEmptyLight();
    void accentPaintsPerLedAndModeColors();
    void accentLeavesColorlessModesUnchanged();
};
