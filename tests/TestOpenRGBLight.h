#pragma once

#include <QObject>

class TestOpenRGBLight : public QObject
{
    Q_OBJECT

private slots:
    void keyUsesNameAndSerial();
    void readsDeviceModeAndZoneLeds();
    void readsZoneModeWhenSet();
    void applyWritesOnlyChangedLeds();
    void applyUpdatesModeColors();
    void applySkipsWhenModeChanged();
    void effectsOnlyZoneReadsOnlyItsColors();
    void applyRestoresModeWhenAsked();
    void applyRestoresZoneModeWhenAsked();
};
