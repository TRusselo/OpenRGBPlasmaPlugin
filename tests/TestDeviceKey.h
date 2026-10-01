#pragma once

#include <QObject>

class TestDeviceKey : public QObject
{
    Q_OBJECT

private slots:
    void usesSerialWhenPresent();
    void trimsSerial();
    void fallsBackToLocationForNone();
    void fallsBackToLocationForBlankSerial();
};
