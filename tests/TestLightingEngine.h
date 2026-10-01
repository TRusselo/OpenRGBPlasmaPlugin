#pragma once

#include <QObject>

class TestLightingEngine : public QObject
{
    Q_OBJECT

private slots:
    void startupAtFullLevelWritesNothing();
    void levelDimsLightsTickedForDim();
    void levelBeforeLightsAppliesToNewLights();
    void levelIsClamped();
    void levelZeroThenUpRestoresExactly();
    void outsideChangeBecomesBaseAndStopsFollowing();
    void levelAfterOutsideChangeDimsNewBase();
    void ownWritesAreIgnored();
    void outsideWriteDuringOwnWriteIsDetected();
    void supersededWriteKeepsWaiting();
    void accentPaintsTickedLightsAtLevel();
    void accentLeavesOffModeAlone();
    void accentOffWritesNothing();
    void untickingDimRestoresFullBase();
    void tickingDimFollowsSlider();
    void tickingAccentAppliesCurrentAccent();
    void removedLightIsForgotten();
    void emptyLightIsHarmless();
    void staleEchoOfOlderWriteIsIgnored();
    void outOfOrderAckIsIgnored();
    void reshapedLightReadsFreshBase();
    void newLightGetsAccent();
    void newLightWithoutAccentTickIsLeftAlone();
    void newBlackLightTakesOthersColor();
    void newBlackLightIsDimmedToLevel();
    void unlitLightFillsOnNextSliderMove();
    void turnedOffLightStaysOff();
    void untickedBlackLightIsLeftAlone();
    void offModeLightIsLeftAlone();
    void accentClearsUnlit();
    void rescannedLightGetsStateBack();
};
