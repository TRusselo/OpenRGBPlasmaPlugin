#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <QObject>
#include <QString>
#include <QTimer>

#include "Light.h"
#include "LightMetaType.h"

class DeviceWriter : public QObject
{
    Q_OBJECT

public:
    explicit DeviceWriter(int intervalMs = 30, QObject* parent = nullptr);

    void setLights(const std::vector<std::shared_ptr<Light>>& lights);

public slots:
    void enqueue(const QString& key, const LightState& target, quint64 sequence);

signals:
    void written(const QString& key, quint64 sequence);

private:
    void flush();

    std::map<std::string, std::shared_ptr<Light>> lights;
    struct Pending
    {
        LightState target;
        quint64 sequence = 0;
    };

    std::map<std::string, Pending> pending;
    QTimer timer;
};
