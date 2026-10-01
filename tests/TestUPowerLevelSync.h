#pragma once

#include <QObject>

class TestUPowerLevelSync : public QObject
{
    Q_OBJECT

private slots:
    void ownPathMatchesNativePathName();
    void onlyOwnBacklightSetsSharedLevelToMaximum();
    void otherBacklightsShareTheirLevel();
    void ownNotExportedYetWritesNothing();
};
