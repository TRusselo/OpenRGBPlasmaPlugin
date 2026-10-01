#pragma once

#include <QObject>

#include "OpenRGBPluginInterface.h"

class PlasmaIntegrationPlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID FILE "PlasmaIntegrationPlugin.json")
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    ~PlasmaIntegrationPlugin() override = default;

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
    OpenRGBPluginAPIInterface* api = nullptr;
    QWidget* tab = nullptr;
};
