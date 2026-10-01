#include "DeviceWriter.h"

#include <iterator>

DeviceWriter::DeviceWriter(int intervalMs, QObject* parent)
    : QObject(parent)
    , timer(this)
{
    timer.setSingleShot(true);
    timer.setInterval(intervalMs);
    connect(&timer, &QTimer::timeout, this, &DeviceWriter::flush);
}

void DeviceWriter::setLights(const std::vector<std::shared_ptr<Light>>& newLights)
{
    lights.clear();
    for(const std::shared_ptr<Light>& light : newLights)
    {
        lights[light->key()] = light;
    }
    for(auto entry = pending.begin(); entry != pending.end();)
    {
        entry = lights.count(entry->first) ? std::next(entry) : pending.erase(entry);
    }
}

void DeviceWriter::enqueue(const QString& key, const LightState& target, quint64 sequence)
{
    pending[key.toStdString()] = Pending{target, sequence};
    if(!timer.isActive())
    {
        timer.start();
    }
}

void DeviceWriter::flush()
{
    std::map<std::string, Pending> batch;
    batch.swap(pending);
    for(const auto& [key, write] : batch)
    {
        const auto found = lights.find(key);
        if(found == lights.end())
        {
            continue;
        }
        found->second->apply(write.target);
        emit written(QString::fromStdString(key), write.sequence);
    }
}
