#include "AccentColorSource.h"

#include <QColor>
#include <QDBusConnection>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>

AccentColorSource::AccentColorSource(const QString& kdeglobalsPath, QObject* parent)
    : QObject(parent)
    , path(kdeglobalsPath)
    , watcher(this)
{
}

QString AccentColorSource::defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/kdeglobals");
}

std::optional<Rgb> AccentColorSource::parseColor(const QString& text)
{
    const QString trimmed = text.trimmed();
    if(trimmed.startsWith(QLatin1Char('#')))
    {
        const QColor color(trimmed);
        if(!color.isValid())
        {
            return std::nullopt;
        }
        return makeRgb((unsigned int)color.red(), (unsigned int)color.green(), (unsigned int)color.blue());
    }

    const QStringList parts = trimmed.split(QLatin1Char(','));
    if(parts.size() != 3 && parts.size() != 4)
    {
        return std::nullopt;
    }
    unsigned int channels[3];
    for(int i = 0; i < 3; i++)
    {
        bool ok = false;
        const int channel = parts[i].trimmed().toInt(&ok);
        if(!ok || channel < 0 || channel > 255)
        {
            return std::nullopt;
        }
        channels[i] = (unsigned int)channel;
    }
    return makeRgb(channels[0], channels[1], channels[2]);
}

std::optional<Rgb> AccentColorSource::readAccent(const QString& filePath)
{
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return std::nullopt;
    }

    QString group;
    std::optional<Rgb> custom;
    std::optional<Rgb> scheme;
    while(!file.atEnd())
    {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if(line.startsWith(QLatin1Char('[')))
        {
            const int end = line.indexOf(QLatin1Char(']'));
            group = (end > 0) ? line.mid(1, end - 1) : QString();
            continue;
        }
        const int equals = line.indexOf(QLatin1Char('='));
        if(equals <= 0)
        {
            continue;
        }
        const QString key = line.left(equals).trimmed();
        const QString text = line.mid(equals + 1);
        if(group == QLatin1String("General") && key == QLatin1String("AccentColor"))
        {
            custom = parseColor(text);
        }
        else if(group == QLatin1String("Colors:View") && key == QLatin1String("ForegroundActive"))
        {
            scheme = parseColor(text);
        }
    }

    if(custom)
    {
        return custom;
    }
    if(scheme)
    {
        return scheme;
    }
    return makeRgb(255, 255, 255);
}

void AccentColorSource::start()
{
    QDBusConnection::sessionBus().connect(QString(), QStringLiteral("/kdeglobals"), QStringLiteral("org.kde.kconfig.notify"), QStringLiteral("ConfigChanged"), this, SLOT(reload()));
    connect(&watcher, &QFileSystemWatcher::fileChanged, this, &AccentColorSource::reload);
    connect(&watcher, &QFileSystemWatcher::directoryChanged, this, &AccentColorSource::reload);
    watcher.addPath(QFileInfo(path).absolutePath());
    reload();
}

std::optional<Rgb> AccentColorSource::current() const
{
    return value;
}

void AccentColorSource::reload()
{
    if(QFileInfo::exists(path) && !watcher.files().contains(path))
    {
        watcher.addPath(path);
    }
    const std::optional<Rgb> latest = readAccent(path);
    if(started && latest == value)
    {
        return;
    }
    started = true;
    value = latest;
    emit accentChanged(value.has_value(), value.value_or(0));
}
