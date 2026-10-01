#include "PlasmaIntegrationPlugin.h"

#include <QLabel>

OpenRGBPluginInfo PlasmaIntegrationPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info;
    info.Name = "Plasma Integration";
    info.Description = "Control OpenRGB lights from KDE Plasma's keyboard backlight slider and accent color";
    info.Version = PLUGIN_VERSION;
    info.Commit = GIT_COMMIT_ID;
    info.URL = "https://github.com/TRusselo/OpenRGBPlasmaPlugin";
    info.Location = OPENRGB_PLUGIN_LOCATION_TOP;
    info.Label = "Plasma Integration";
    info.ProtocolVersion = 0;
    return info;
}

unsigned int PlasmaIntegrationPlugin::GetPluginAPIVersion()
{
    return OPENRGB_PLUGIN_API_VERSION;
}

void PlasmaIntegrationPlugin::Load(OpenRGBPluginAPIInterface* plugin_api)
{
    api = plugin_api;
    tab = new QLabel(QStringLiteral("Plasma Integration"));
}

QWidget* PlasmaIntegrationPlugin::GetWidget()
{
    return tab;
}

QMenu* PlasmaIntegrationPlugin::GetTrayMenu()
{
    return nullptr;
}

void PlasmaIntegrationPlugin::Unload()
{
}

void PlasmaIntegrationPlugin::OnProfileAboutToLoad()
{
}

void PlasmaIntegrationPlugin::OnProfileLoad(nlohmann::json)
{
}

nlohmann::json PlasmaIntegrationPlugin::OnProfileSave()
{
    return nlohmann::json();
}

unsigned char* PlasmaIntegrationPlugin::OnSDKCommand(unsigned int, unsigned char*, unsigned int* pkt_size)
{
    if(pkt_size)
    {
        *pkt_size = 0;
    }
    return nullptr;
}

void PlasmaIntegrationPlugin::ProfileManagerUpdated(unsigned int)
{
}

void PlasmaIntegrationPlugin::ResourceManagerUpdated(unsigned int)
{
}

void PlasmaIntegrationPlugin::SettingsManagerUpdated(unsigned int)
{
}
