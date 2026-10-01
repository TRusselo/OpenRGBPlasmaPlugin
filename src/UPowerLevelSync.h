#pragma once

#include <optional>

#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

class QDBusObjectPath;
class QTimer;

class UPowerLevelSync : public QObject
{
    Q_OBJECT

public:
    struct Write
    {
        QString path;
        int value = 0;
    };

    static inline const QString CompositePath = QStringLiteral("/org/freedesktop/UPower/KbdBacklight");

    explicit UPowerLevelSync(const QString& ledName, int ownMaximum, QObject* parent = nullptr);

    void start();

    static QString ownPath(const QMap<QString, QString>& nativePaths, const QString& ledName);
    static std::optional<Write> plan(const QStringList& backlights, const QString& own, int sharedLevel, int sharedMaximum, int ownMaximum);

private slots:
    void onDeviceAdded(const QDBusObjectPath& path);

private:
    void trySync();
    void stop();

    QString name;
    int maximum;
    QTimer* deadline;
    bool listening = false;
};
