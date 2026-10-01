#include "TestSettingsTab.h"

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTest>

#include "SettingsTab.h"

namespace
{
std::vector<SettingsTab::DeviceRow> rows()
{
    return {{"Keyboard|A1", "Razer Blackwidow Elite"}, {"Mat|B2", "Razer Goliathus"}};
}

PluginSettings settingsWithMatUndimmed()
{
    PluginSettings settings;
    settings.devices["Mat|B2"] = DeviceOptions{false, true};
    return settings;
}
}

void TestSettingsTab::rowsReflectSettings()
{
    SettingsTab tab;
    tab.setDevices(rows(), settingsWithMatUndimmed());
    auto* table = tab.findChild<QTableWidget*>(QStringLiteral("deviceTable"));
    QVERIFY(table);
    QCOMPARE(table->rowCount(), 2);
    QCOMPARE(table->item(0, 0)->text(), QStringLiteral("Razer Blackwidow Elite"));
    QCOMPARE(table->item(0, 1)->checkState(), Qt::Checked);
    QCOMPARE(table->item(1, 1)->checkState(), Qt::Unchecked);
    QCOMPARE(table->item(1, 2)->checkState(), Qt::Checked);
}

void TestSettingsTab::togglingDimEmitsOptions()
{
    SettingsTab tab;
    tab.setDevices(rows(), PluginSettings());
    QSignalSpy spy(&tab, &SettingsTab::deviceOptionsChanged);
    auto* table = tab.findChild<QTableWidget*>(QStringLiteral("deviceTable"));
    QVERIFY(table);
    QCOMPARE(table->rowCount(), 2);

    table->item(1, 1)->setCheckState(Qt::Unchecked);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("Mat|B2"));
    QCOMPARE(spy.at(0).at(1).toBool(), false);
    QCOMPARE(spy.at(0).at(2).toBool(), true);
}

void TestSettingsTab::fillingRowsEmitsNothing()
{
    SettingsTab tab;
    QSignalSpy spy(&tab, &SettingsTab::deviceOptionsChanged);
    tab.setDevices(rows(), settingsWithMatUndimmed());
    tab.setDevices(rows(), PluginSettings());
    QCOMPARE(spy.count(), 0);
}

void TestSettingsTab::accentCheckboxEmits()
{
    SettingsTab tab;
    QSignalSpy spy(&tab, &SettingsTab::accentToggled);
    tab.setAccentEnabled(true);
    QCOMPARE(spy.count(), 0);
    auto* box = tab.findChild<QCheckBox*>(QStringLiteral("accentCheck"));
    QVERIFY(box);
    QVERIFY(box->isChecked());
    box->setChecked(false);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), false);
}

void TestSettingsTab::statusAndButtons()
{
    SettingsTab tab;
    tab.setStatusLines({QStringLiteral("one"), QStringLiteral("two")});
    auto* status = tab.findChild<QLabel*>(QStringLiteral("statusLabel"));
    QVERIFY(status);
    QCOMPARE(status->text(), QStringLiteral("one\ntwo"));

    auto* setup = tab.findChild<QPushButton*>(QStringLiteral("setupButton"));
    QVERIFY(setup);
    QVERIFY(setup->isHidden());
    tab.setSetupVisible(true);
    QVERIFY(!setup->isHidden());
    QSignalSpy setupSpy(&tab, &SettingsTab::setupRequested);
    setup->click();
    QCOMPARE(setupSpy.count(), 1);

    auto* restart = tab.findChild<QPushButton*>(QStringLiteral("restartButton"));
    QVERIFY(restart);
    QVERIFY(restart->isHidden());
    tab.setRestartVisible(true);
    QSignalSpy restartSpy(&tab, &SettingsTab::restartRequested);
    restart->click();
    QCOMPARE(restartSpy.count(), 1);

    auto* manual = tab.findChild<QLabel*>(QStringLiteral("manualCommands"));
    QVERIFY(manual);
    QVERIFY(manual->isHidden());
    tab.setManualCommands(QStringLiteral("sudo modprobe uleds"));
    QVERIFY(!manual->isHidden());
    tab.setManualCommands(QString());
    QVERIFY(manual->isHidden());
}
