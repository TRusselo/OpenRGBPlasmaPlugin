#pragma once

#include <QObject>

class TestPluginSettings : public QObject
{
    Q_OBJECT

private slots:
    void defaultsWhenEmpty();
    void readsStoredValues();
    void wrongTypesFallBackToDefaults();
    void roundTripsThroughJson();
};
