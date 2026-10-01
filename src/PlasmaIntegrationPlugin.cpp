#include "PlasmaIntegrationPlugin.h"

#include <QTimer>

#include "AccentColorSource.h"
#include "DeviceWriter.h"
#include "LightingEngine.h"
#include "LogManager.h"
#include "OpenRGBLight.h"
#include "PowerDevilProbe.h"
#include "ResourceManagerCallback.h"
#include "RGBControllerInterface.h"
#include "SettingsTab.h"
#include "StatusText.h"
#include "SystemSetup.h"
#include "UledsBacklight.h"
#include "UPowerLevelSync.h"

PlasmaIntegrationPlugin::PlasmaIntegrationPlugin() = default;

PlasmaIntegrationPlugin::~PlasmaIntegrationPlugin()
{
    Unload();
}

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
    qRegisterMetaType<LightState>("LightState");
    settings = PluginSettings::fromJson(api->GetSettings(SettingsKey));

    tab = new SettingsTab();
    backlight = new UledsBacklight();
    backlight->setParent(this);
    if(backlight->ledExists())
    {
        tab->setPassive(true);
        tab->setStatusLines({QStringLiteral("Another OpenRGB instance owns the backlight."),
                             QStringLiteral("This window leaves the lights alone; use the OpenRGB window that started first.")});
        api->LogEntry(__FILE__, __LINE__, LL_INFO, "[PlasmaIntegration] another instance owns the backlight, staying passive");
        return;
    }

    writer = new DeviceWriter();
    writer->moveToThread(&writerThread);
    connect(&writerThread, &QThread::finished, writer, &QObject::deleteLater);
    writerThread.start();

    engine = std::make_unique<LightingEngine>([this](const std::string& key, const LightState& target, std::uint64_t sequence, bool restoreMode) {
        QMetaObject::invokeMethod(writer, "enqueue", Qt::QueuedConnection, Q_ARG(QString, QString::fromStdString(key)), Q_ARG(LightState, target),
                                  Q_ARG(quint64, quint64(sequence)), Q_ARG(bool, restoreMode));
    });
    engine->setSettings(settings);
    connect(writer, &DeviceWriter::written, this, [this](const QString& key, quint64 sequence) {
        if(engine)
        {
            engine->onWriteFinished(key.toStdString(), sequence);
        }
    });

    setup = new SystemSetup(this);
    levelSync = new UPowerLevelSync(QStringLiteral("openrgb::kbd_backlight"), UledsBacklight::MaxBrightness, this);
    probe = new PowerDevilProbe(this);
    accentSource = new AccentColorSource(AccentColorSource::defaultPath(), this);

    connect(backlight, &UledsBacklight::levelChanged, this, [this](int level) { engine->setLevel(level); });
    connect(accentSource, &AccentColorSource::accentChanged, this, [this](bool, unsigned int) {
        applyAccent();
        updateStatus();
    });
    connect(tab, &SettingsTab::setupRequested, setup, &SystemSetup::runSetup);
    connect(setup, &SystemSetup::setupFinished, this, [this](bool success, const QString& message) {
        if(success)
        {
            tab->setManualCommands(QString());
            openBacklight();
        }
        else
        {
            tab->setManualCommands(message + QLatin1Char('\n') + SystemSetup::manualCommands());
        }
        updateStatus();
    });
    connect(tab, &SettingsTab::restartRequested, probe, &PowerDevilProbe::restartPowerDevil);
    connect(tab, &SettingsTab::shown, this, [this] { probe->check(backlight->status() == UledsBacklight::Status::Ready); });
    connect(probe, &PowerDevilProbe::restartNeededChanged, this, [this](bool) { updateStatus(); });
    connect(tab, &SettingsTab::accentToggled, this, [this](bool enabled) {
        settings.accentEnabled = enabled;
        saveSettings();
        applyAccent();
    });
    connect(tab, &SettingsTab::deviceOptionsChanged, this, [this](const QString& key, bool dim, bool accent) {
        settings.devices[key.toStdString()] = DeviceOptions{dim, accent};
        saveSettings();
        engine->setSettings(settings);
    });

    tab->setAccentEnabled(settings.accentEnabled);
    refreshLights();
    accentSource->start();
    openBacklight();
    api->LogEntry(__FILE__, __LINE__, LL_INFO, "[PlasmaIntegration] loaded, %zu devices", lights.size());
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
    if(!api)
    {
        return;
    }
    for(RGBControllerInterface* controller : api->GetRGBControllers())
    {
        controller->UnregisterUpdateCallback(this);
    }
    keysByController.clear();
    if(backlight)
    {
        backlight->close();
    }
    writerThread.quit();
    writerThread.wait();
    writer = nullptr;
    engine.reset();
    lights.clear();
    api = nullptr;
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

void PlasmaIntegrationPlugin::ResourceManagerUpdated(unsigned int update_reason)
{
    if(update_reason != RESOURCEMANAGER_UPDATE_REASON_DEVICE_LIST_UPDATED || !engine)
    {
        return;
    }
    if(QThread::currentThread() == thread())
    {
        refreshLights();
    }
    else
    {
        QMetaObject::invokeMethod(this, [this] { refreshLights(); }, Qt::BlockingQueuedConnection);
    }
}

void PlasmaIntegrationPlugin::SettingsManagerUpdated(unsigned int)
{
}

void PlasmaIntegrationPlugin::controllerCallback(void* plugin, unsigned int reason, void* controller)
{
    auto* self = static_cast<PlasmaIntegrationPlugin*>(plugin);
    switch(reason)
    {
        case RGBCONTROLLER_UPDATE_REASON_UPDATELEDS:
        case RGBCONTROLLER_UPDATE_REASON_UPDATEMODE:
        case RGBCONTROLLER_UPDATE_REASON_SAVEMODE:
            QMetaObject::invokeMethod(self, [self, controller] { self->onControllerChanged(controller); }, Qt::QueuedConnection);
            break;
        case RGBCONTROLLER_UPDATE_REASON_HIDDEN:
        case RGBCONTROLLER_UPDATE_REASON_UNHIDDEN:
        case RGBCONTROLLER_UPDATE_REASON_CONFIGUREZONE:
        case RGBCONTROLLER_UPDATE_REASON_DEVICE_CHANGED:
            QMetaObject::invokeMethod(self, [self] { self->refreshLights(); }, Qt::QueuedConnection);
            break;
        default:
            break;
    }
}

void PlasmaIntegrationPlugin::refreshLights()
{
    if(!engine)
    {
        return;
    }
    keysByController.clear();
    lights.clear();
    std::vector<std::shared_ptr<Light>> handles;
    std::vector<SettingsTab::DeviceRow> rows;
    for(RGBControllerInterface* controller : api->GetRGBControllers())
    {
        controller->UnregisterUpdateCallback(this);
        if(controller->GetHidden())
        {
            continue;
        }
        auto light = std::make_shared<OpenRGBLight>(controller);
        controller->RegisterUpdateCallback(&PlasmaIntegrationPlugin::controllerCallback, this);
        keysByController[controller] = light->key();
        lights.push_back(light);
        handles.push_back(light);
        rows.push_back({light->key(), light->name()});
    }
    QMetaObject::invokeMethod(writer, [this, handles] { writer->setLights(handles); }, Qt::BlockingQueuedConnection);
    engine->setLights(handles);
    tab->setDevices(rows, settings);
}

void PlasmaIntegrationPlugin::onControllerChanged(void* controller)
{
    const auto found = keysByController.find(controller);
    if(found != keysByController.end() && engine)
    {
        engine->onLightChanged(found->second);
    }
}

void PlasmaIntegrationPlugin::openBacklight()
{
    if(SystemSetup::detect(SystemSetup::systemPaths()) == SystemSetup::State::Ready && backlight->open() == UledsBacklight::Status::Ready)
    {
        levelSync->start();
    }
    updateStatus();
    checkPowerDevilSoon();
}

void PlasmaIntegrationPlugin::checkPowerDevilSoon()
{
    QTimer::singleShot(3000, this, [this] { probe->check(backlight->status() == UledsBacklight::Status::Ready); });
}

void PlasmaIntegrationPlugin::updateStatus()
{
    StatusInput input;
    input.setup = SystemSetup::detect(SystemSetup::systemPaths());
    input.backlight = backlight->status();
    input.accentAvailable = accentSource->current().has_value();
    input.powerDevilNeedsRestart = probe->restartNeeded();
    tab->setStatusLines(statusLines(input));
    tab->setSetupVisible(input.setup == SystemSetup::State::NeedsSetup && input.backlight != UledsBacklight::Status::Ready);
    tab->setRestartVisible(input.powerDevilNeedsRestart);
}

void PlasmaIntegrationPlugin::saveSettings()
{
    api->SetSettings(SettingsKey, settings.toJson());
    api->SaveSettings();
}

void PlasmaIntegrationPlugin::applyAccent()
{
    if(engine)
    {
        engine->setAccent(settings.accentEnabled ? accentSource->current() : std::nullopt);
    }
}
