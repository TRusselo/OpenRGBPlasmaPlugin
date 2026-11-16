#pragma once

#include <QObject>

class PowerDevilProbe : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    static bool needsRestart(bool backlightExists, bool powerDevilRunning, bool supported, int maxBrightness);
    static bool shouldRestartAutomatically(bool restartNeeded, bool alreadyRestarted);
    bool restartNeeded() const;

public slots:
    void check(bool backlightExists);
    void restartPowerDevil();
    // Restarts PowerDevil without asking if it missed the backlight, at most once per backlight.
    void restartIfMissed(bool backlightExists);
    // Call when a new backlight is created, so restartIfMissed may restart once more.
    void allowAutomaticRestart();

signals:
    void restartNeededChanged(bool needed);

private:
    bool needed = false;
    bool lastBacklightExists = false;
    bool restartedAutomatically = false;
};
