#pragma once

#include <optional>

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>

#include "LightModel.h"

class AccentColorSource : public QObject
{
    Q_OBJECT

public:
    explicit AccentColorSource(const QString& kdeglobalsPath = defaultPath(), QObject* parent = nullptr);

    static QString defaultPath();
    static std::optional<Rgb> readAccent(const QString& path);
    static std::optional<Rgb> parseColor(const QString& value);

    void start();
    std::optional<Rgb> current() const;

signals:
    void accentChanged(bool available, unsigned int rgb);

private slots:
    void reload();

private:
    QString path;
    std::optional<Rgb> value;
    bool started = false;
    QFileSystemWatcher watcher;
};
