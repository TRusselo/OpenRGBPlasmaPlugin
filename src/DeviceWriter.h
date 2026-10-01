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
    void enqueue(const QString& key, const LightState& target);

signals:
    void written(const QString& key, const LightState& applied);

private:
    void flush();

    std::map<std::string, std::shared_ptr<Light>> lights;
    std::map<std::string, LightState> pending;
    QTimer timer;
};
