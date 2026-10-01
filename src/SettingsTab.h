#pragma once

#include <string>
#include <vector>

#include <QWidget>

#include "PluginSettings.h"

class QCheckBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;

class SettingsTab : public QWidget
{
    Q_OBJECT

public:
    struct DeviceRow
    {
        std::string key;
        std::string name;
    };

    explicit SettingsTab(QWidget* parent = nullptr);

    void setStatusLines(const QStringList& lines);
    void setSetupVisible(bool visible);
    void setManualCommands(const QString& commands);
    void setRestartVisible(bool visible);
    void setAccentEnabled(bool enabled);
    void setDevices(const std::vector<DeviceRow>& rows, const PluginSettings& settings);

signals:
    void setupRequested();
    void restartRequested();
    void accentToggled(bool enabled);
    void deviceOptionsChanged(const QString& key, bool dim, bool accent);
    void shown();

protected:
    void showEvent(QShowEvent* event) override;

private:
    void onItemChanged(QTableWidgetItem* item);

    QLabel* status = nullptr;
    QPushButton* setupButton = nullptr;
    QLabel* manual = nullptr;
    QPushButton* restartButton = nullptr;
    QCheckBox* accentBox = nullptr;
    QTableWidget* table = nullptr;
    bool updating = false;
};
