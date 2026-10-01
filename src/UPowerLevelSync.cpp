#include "UPowerLevelSync.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusVariant>
#include <QTimer>

namespace
{
const QString Service = QStringLiteral("org.freedesktop.UPower");
const QString ManagerPath = QStringLiteral("/org/freedesktop/UPower");
const QString BacklightInterface = QStringLiteral("org.freedesktop.UPower.KbdBacklight");
constexpr int CallTimeoutMs = 2000;
constexpr int WaitForExportMs = 15000;

int callInt(const QString& path, const char* method)
{
    QDBusInterface backlight(Service, path, BacklightInterface, QDBusConnection::systemBus());
    backlight.setTimeout(CallTimeoutMs);
    const QDBusReply<int> reply = backlight.call(QLatin1String(method));
    return reply.isValid() ? reply.value() : 0;
}
}

UPowerLevelSync::UPowerLevelSync(const QString& ledName, int ownMaximum, QObject* parent)
    : QObject(parent)
    , name(ledName)
    , maximum(ownMaximum)
    , deadline(new QTimer(this))
{
    deadline->setSingleShot(true);
    connect(deadline, &QTimer::timeout, this, &UPowerLevelSync::stop);
}

void UPowerLevelSync::start()
{
    if(!listening)
    {
        listening = QDBusConnection::systemBus().connect(Service, ManagerPath, Service, QStringLiteral("DeviceAdded"), this,
                                                         SLOT(onDeviceAdded(QDBusObjectPath)));
    }
    deadline->start(WaitForExportMs);
    trySync();
}

QString UPowerLevelSync::ownPath(const QMap<QString, QString>& nativePaths, const QString& ledName)
{
    const QString suffix = QLatin1Char('/') + ledName;
    for(auto entry = nativePaths.begin(); entry != nativePaths.end(); ++entry)
    {
        if(entry.value().endsWith(suffix))
        {
            return entry.key();
        }
    }
    return QString();
}

std::optional<UPowerLevelSync::Write> UPowerLevelSync::plan(const QStringList& backlights, const QString& own, int sharedLevel, int sharedMaximum,
                                                             int ownMaximum)
{
    if(own.isEmpty() || !backlights.contains(own))
    {
        return std::nullopt;
    }
    if(backlights.size() == 1)
    {
        return Write{CompositePath, sharedMaximum};
    }
    const int value = sharedMaximum > 0 ? (sharedLevel * ownMaximum + sharedMaximum / 2) / sharedMaximum : ownMaximum;
    return Write{own, value};
}

void UPowerLevelSync::onDeviceAdded(const QDBusObjectPath& path)
{
    if(path.path().startsWith(CompositePath))
    {
        trySync();
    }
}

void UPowerLevelSync::trySync()
{
    QDBusInterface manager(Service, ManagerPath, Service, QDBusConnection::systemBus());
    manager.setTimeout(CallTimeoutMs);
    const QDBusReply<QList<QDBusObjectPath>> listed = manager.call(QStringLiteral("EnumerateKbdBacklights"));
    if(!listed.isValid())
    {
        return;
    }
    QStringList backlights;
    QMap<QString, QString> nativePaths;
    for(const QDBusObjectPath& path : listed.value())
    {
        backlights << path.path();
        QDBusInterface properties(Service, path.path(), QStringLiteral("org.freedesktop.DBus.Properties"), QDBusConnection::systemBus());
        properties.setTimeout(CallTimeoutMs);
        const QDBusReply<QDBusVariant> native = properties.call(QStringLiteral("Get"), BacklightInterface, QStringLiteral("NativePath"));
        if(native.isValid())
        {
            nativePaths[path.path()] = native.value().variant().toString();
        }
    }
    const QString own = ownPath(nativePaths, name);
    const std::optional<Write> write =
        plan(backlights, own, callInt(CompositePath, "GetBrightness"), callInt(CompositePath, "GetMaxBrightness"), maximum);
    if(!write)
    {
        return;
    }
    QDBusInterface target(Service, write->path, BacklightInterface, QDBusConnection::systemBus());
    target.setTimeout(CallTimeoutMs);
    target.call(QStringLiteral("SetBrightness"), write->value);
    stop();
}

void UPowerLevelSync::stop()
{
    deadline->stop();
    if(listening)
    {
        QDBusConnection::systemBus().disconnect(Service, ManagerPath, Service, QStringLiteral("DeviceAdded"), this, SLOT(onDeviceAdded(QDBusObjectPath)));
        listening = false;
    }
}
