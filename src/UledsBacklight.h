#pragma once

#include <optional>

#include <QByteArray>
#include <QObject>
#include <QString>

class QSocketNotifier;

class UledsBacklight : public QObject
{
    Q_OBJECT

public:
    enum class Status
    {
        Closed,
        Ready,
        Missing,
        NoPermission,
        NameTaken,
        Failed,
    };

    static constexpr int MaxBrightness = 100;

    explicit UledsBacklight(const QString& ledName = QStringLiteral("openrgb::kbd_backlight"),
                            const QString& devicePath = QStringLiteral("/dev/uleds"),
                            const QString& ledsDir = QStringLiteral("/sys/class/leds"),
                            QObject* parent = nullptr);
    ~UledsBacklight() override;

    Status open();
    void close();
    Status status() const;
    bool ledExists() const;

    static QByteArray registration(const QString& name, int maxBrightness);
    static std::optional<int> parseLevel(const QByteArray& data);

signals:
    void levelChanged(int level);

private:
    void onReadable();

    QString name;
    QString device;
    QString ledsDirectory;
    int fd = -1;
    QSocketNotifier* notifier = nullptr;
    Status current = Status::Closed;
};
