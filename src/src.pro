include(../common.pri)

TEMPLATE = lib
CONFIG += plugin
TARGET = OpenRGBPlasmaPlugin
QT += core gui widgets dbus

PLUGIN_VERSION = 0.1.0
GIT_COMMIT_ID = $$system(git -C $$PWD/.. rev-parse --short HEAD)

DEFINES += \
    PLUGIN_VERSION=\\\"$$PLUGIN_VERSION\\\" \
    GIT_COMMIT_ID=\\\"$$GIT_COMMIT_ID\\\"

INCLUDEPATH += \
    $$PWD/../OpenRGB \
    $$PWD/../OpenRGB/RGBController \
    $$PWD/../OpenRGB/qt

HEADERS += \
    PlasmaIntegrationPlugin.h \
    LightModel.h \
    Dimming.h \
    DeviceKey.h \
    PluginSettings.h \
    Light.h \
    LightingEngine.h \
    LightMetaType.h \
    DeviceWriter.h \
    UledsBacklight.h \
    AccentColorSource.h \
    SystemSetup.h \
    PowerDevilProbe.h \
    StatusText.h \
    SettingsTab.h \
    OpenRGBLight.h \
    UPowerLevelSync.h

SOURCES += \
    PlasmaIntegrationPlugin.cpp \
    Dimming.cpp \
    DeviceKey.cpp \
    PluginSettings.cpp \
    LightingEngine.cpp \
    DeviceWriter.cpp \
    UledsBacklight.cpp \
    AccentColorSource.cpp \
    SystemSetup.cpp \
    PowerDevilProbe.cpp \
    StatusText.cpp \
    SettingsTab.cpp \
    OpenRGBLight.cpp \
    UPowerLevelSync.cpp

DISTFILES += \
    PlasmaIntegrationPlugin.json
