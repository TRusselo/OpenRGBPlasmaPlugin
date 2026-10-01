#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <QObject>
#include <QThread>

#include "OpenRGBPluginInterface.h"
#include "PluginSettings.h"

class AccentColorSource;
class DeviceWriter;
class LightingEngine;
class OpenRGBLight;
class PowerDevilProbe;
class SettingsTab;
class SystemSetup;
class UledsBacklight;
class UPowerLevelSync;

class PlasmaIntegrationPlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID FILE "PlasmaIntegrationPlugin.json")
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    PlasmaIntegrationPlugin();
    ~PlasmaIntegrationPlugin() override;

    OpenRGBPluginInfo GetPluginInfo() override;
    unsigned int GetPluginAPIVersion() override;
    void Load(OpenRGBPluginAPIInterface* plugin_api) override;
    QWidget* GetWidget() override;
    QMenu* GetTrayMenu() override;
    void Unload() override;
    void OnProfileAboutToLoad() override;
    void OnProfileLoad(nlohmann::json profile_data) override;
    nlohmann::json OnProfileSave() override;
    unsigned char* OnSDKCommand(unsigned int pkt_id, unsigned char* pkt_data, unsigned int* pkt_size) override;
    void ProfileManagerUpdated(unsigned int update_reason) override;
    void ResourceManagerUpdated(unsigned int update_reason) override;
    void SettingsManagerUpdated(unsigned int update_reason) override;

private:
    static void controllerCallback(void* plugin, unsigned int reason, void* controller);

    void refreshLights();
    void onControllerChanged(void* controller);
    void openBacklight();
    void checkPowerDevilSoon();
    void updateStatus();
    void saveSettings();
    void applyAccent();

    OpenRGBPluginAPIInterface* api = nullptr;
    PluginSettings settings;
    std::unique_ptr<LightingEngine> engine;
    QThread writerThread;
    DeviceWriter* writer = nullptr;
    UledsBacklight* backlight = nullptr;
    UPowerLevelSync* levelSync = nullptr;
    AccentColorSource* accentSource = nullptr;
    SystemSetup* setup = nullptr;
    PowerDevilProbe* probe = nullptr;
    SettingsTab* tab = nullptr;
    std::vector<std::shared_ptr<OpenRGBLight>> lights;
    std::map<void*, std::string> keysByController;
};
