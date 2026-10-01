#include "UledsBacklight.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include <QFile>
#include <QFileInfo>
#include <QSocketNotifier>

namespace
{
constexpr int NameSize = 64;
}

UledsBacklight::UledsBacklight(const QString& ledName, const QString& devicePath, const QString& ledsDir, QObject* parent)
    : QObject(parent)
    , name(ledName)
    , device(devicePath)
    , ledsDirectory(ledsDir)
{
}

UledsBacklight::~UledsBacklight()
{
    close();
}

UledsBacklight::Status UledsBacklight::open()
{
    if(fd >= 0)
    {
        return current;
    }
    if(ledExists())
    {
        current = Status::NameTaken;
        return current;
    }

    fd = ::open(QFile::encodeName(device).constData(), O_RDWR | O_CLOEXEC);
    if(fd < 0)
    {
        current = (errno == ENOENT) ? Status::Missing : (errno == EACCES || errno == EPERM) ? Status::NoPermission : Status::Failed;
        return current;
    }

    const QByteArray request = registration(name, MaxBrightness);
    if(::write(fd, request.constData(), size_t(request.size())) != ssize_t(request.size()))
    {
        ::close(fd);
        fd = -1;
        current = Status::Failed;
        return current;
    }

    notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &UledsBacklight::onReadable);
    current = Status::Ready;
    return current;
}

void UledsBacklight::close()
{
    delete notifier;
    notifier = nullptr;
    if(fd >= 0)
    {
        ::close(fd);
        fd = -1;
    }
    current = Status::Closed;
}

UledsBacklight::Status UledsBacklight::status() const
{
    return current;
}

bool UledsBacklight::ledExists() const
{
    return QFileInfo::exists(ledsDirectory + QLatin1Char('/') + name);
}

QByteArray UledsBacklight::registration(const QString& ledName, int maxBrightness)
{
    QByteArray buffer(NameSize + int(sizeof(int)), '\0');
    const QByteArray encoded = ledName.toUtf8().left(NameSize - 1);
    std::memcpy(buffer.data(), encoded.constData(), size_t(encoded.size()));
    std::memcpy(buffer.data() + NameSize, &maxBrightness, sizeof(int));
    return buffer;
}

std::optional<int> UledsBacklight::parseLevel(const QByteArray& data)
{
    if(data.size() != int(sizeof(int)))
    {
        return std::nullopt;
    }
    int value = 0;
    std::memcpy(&value, data.constData(), sizeof(int));
    return value;
}

void UledsBacklight::onReadable()
{
    QByteArray data(int(sizeof(int)), '\0');
    if(::read(fd, data.data(), size_t(data.size())) != ssize_t(sizeof(int)))
    {
        return;
    }
    if(const std::optional<int> level = parseLevel(data))
    {
        emit levelChanged(*level);
    }
}
