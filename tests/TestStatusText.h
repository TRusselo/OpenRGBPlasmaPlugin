#pragma once

#include <QObject>

class TestStatusText : public QObject
{
    Q_OBJECT

private slots:
    void readyAndAccent();
    void needsSetup();
    void noKernelSupport();
    void nameTaken();
    void accentUnavailableAndPowerDevil();
};
