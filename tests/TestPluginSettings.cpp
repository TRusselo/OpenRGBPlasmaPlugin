#include "TestPluginSettings.h"

#include <QTest>

#include "PluginSettings.h"

void TestPluginSettings::defaultsWhenEmpty()
{
    const PluginSettings settings = PluginSettings::fromJson(nlohmann::json());
    QCOMPARE(settings.accentEnabled, false);
    QCOMPARE(settings.optionsFor("anything").dim, true);
    QCOMPARE(settings.optionsFor("anything").accent, true);
}

void TestPluginSettings::readsStoredValues()
{
    const nlohmann::json json = {{"accent_enabled", true}, {"devices", {{"Fan|1", {{"dim", false}, {"accent", true}}}}}};
    const PluginSettings settings = PluginSettings::fromJson(json);
    QCOMPARE(settings.accentEnabled, true);
    QCOMPARE(settings.optionsFor("Fan|1").dim, false);
    QCOMPARE(settings.optionsFor("Fan|1").accent, true);
}

void TestPluginSettings::wrongTypesFallBackToDefaults()
{
    const nlohmann::json json = {{"accent_enabled", "yes"}, {"devices", {{"Fan|1", {{"dim", 0}, {"accent", "no"}}}, {"Bad|2", 7}}}};
    const PluginSettings settings = PluginSettings::fromJson(json);
    QCOMPARE(settings.accentEnabled, false);
    QCOMPARE(settings.optionsFor("Fan|1").dim, true);
    QCOMPARE(settings.optionsFor("Fan|1").accent, true);
    QCOMPARE(settings.optionsFor("Bad|2").dim, true);
    QCOMPARE(PluginSettings::fromJson(nlohmann::json::array()).accentEnabled, false);
}

void TestPluginSettings::roundTripsThroughJson()
{
    PluginSettings settings;
    settings.accentEnabled = true;
    settings.devices["Keyboard|ABC"] = DeviceOptions{false, false};
    const PluginSettings copy = PluginSettings::fromJson(settings.toJson());
    QCOMPARE(copy.accentEnabled, true);
    QCOMPARE(copy.optionsFor("Keyboard|ABC").dim, false);
    QCOMPARE(copy.optionsFor("Keyboard|ABC").accent, false);
}
