#include "PowerDevilProbe.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QProcess>
#include <QTimer>

namespace
{
const QString Service = QStringLiteral("org.kde.Solid.PowerManagement");

QVariant callPowerDevil(const QString& path, const QString& interface, const QString& method, const QVariantList& arguments)
{
    QDBusMessage call = QDBusMessage::createMethodCall(Service, path, interface, method);
    call.setArguments(arguments);
    const QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 2000);
    if(reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
    {
        return QVariant();
    }
    return reply.arguments().first();
}
}

bool PowerDevilProbe::needsRestart(bool backlightExists, bool powerDevilRunning, bool supported, int maxBrightness)
{
    return backlightExists && powerDevilRunning && (!supported || maxBrightness <= 0);
}

bool PowerDevilProbe::restartNeeded() const
{
    return needed;
}

void PowerDevilProbe::check(bool backlightExists)
{
    lastBacklightExists = backlightExists;
    QDBusConnectionInterface* bus = QDBusConnection::sessionBus().interface();
    const bool running = bus && bus->isServiceRegistered(Service).value();
    bool supported = false;
    int maximum = 0;
    if(running)
    {
        supported = callPowerDevil(QStringLiteral("/org/kde/Solid/PowerManagement"), QStringLiteral("org.kde.Solid.PowerManagement"),
                                   QStringLiteral("isActionSupported"), {QStringLiteral("KeyboardBrightnessControl")})
                        .toBool();
        if(supported)
        {
            maximum = callPowerDevil(QStringLiteral("/org/kde/Solid/PowerManagement/Actions/KeyboardBrightnessControl"),
                                     QStringLiteral("org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl"), QStringLiteral("keyboardBrightnessMax"), {})
                          .toInt();
        }
    }
    const bool now = needsRestart(backlightExists, running, supported, maximum);
    if(now != needed)
    {
        needed = now;
        emit restartNeededChanged(needed);
    }
}

void PowerDevilProbe::restartPowerDevil()
{
    QProcess::startDetached(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("restart"), QStringLiteral("plasma-powerdevil")});
    QTimer::singleShot(5000, this, [this] { check(lastBacklightExists); });
}
