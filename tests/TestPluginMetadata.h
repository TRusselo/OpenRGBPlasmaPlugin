#pragma once

#include <QObject>

class TestPluginMetadata : public QObject
{
    Q_OBJECT

private slots:
    void declaresOpenRGBApiFive();
};
