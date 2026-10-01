#pragma once

#include <QObject>

class TestDeviceWriter : public QObject
{
    Q_OBJECT

private slots:
    void coalescesWritesPerLight();
    void writesEachLight();
    void waitsForInterval();
    void dropsWritesForRemovedLights();
};
