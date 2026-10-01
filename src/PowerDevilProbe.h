#pragma once

#include <QObject>

class PowerDevilProbe : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    static bool needsRestart(bool backlightExists, bool powerDevilRunning, bool supported, int maxBrightness);
    bool restartNeeded() const;

public slots:
    void check(bool backlightExists);
    void restartPowerDevil();

signals:
    void restartNeededChanged(bool needed);

private:
    bool needed = false;
    bool lastBacklightExists = false;
};
