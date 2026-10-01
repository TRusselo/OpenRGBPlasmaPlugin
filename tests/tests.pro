include(../common.pri)

TEMPLATE = app
TARGET = plasma-integration-tests
CONFIG += console
CONFIG -= app_bundle
QT += core gui widgets dbus testlib

DEFINES += PLUGIN_LIBRARY_PATH=\\\"$$OUT_PWD/../src/libOpenRGBPlasmaPlugin.so\\\"

HEADERS += \
    TestPluginMetadata.h \
    LightBuilders.h \
    TestDimming.h \
    ../src/LightModel.h \
    ../src/Dimming.h \
    TestDeviceKey.h \
    TestPluginSettings.h \
    ../src/DeviceKey.h \
    ../src/PluginSettings.h \
    FakeLight.h \
    TestLightingEngine.h \
    ../src/Light.h \
    ../src/LightingEngine.h \
    TestDeviceWriter.h \
    ../src/LightMetaType.h \
    ../src/DeviceWriter.h \
    TestUledsBacklight.h \
    ../src/UledsBacklight.h \
    TestAccentColorSource.h \
    ../src/AccentColorSource.h \
    TestSystemSetup.h \
    TestStatusText.h \
    ../src/SystemSetup.h \
    ../src/PowerDevilProbe.h \
    ../src/StatusText.h \
    TestSettingsTab.h \
    ../src/SettingsTab.h

SOURCES += \
    main.cpp \
    TestPluginMetadata.cpp \
    TestDimming.cpp \
    ../src/Dimming.cpp \
    TestDeviceKey.cpp \
    TestPluginSettings.cpp \
    ../src/DeviceKey.cpp \
    ../src/PluginSettings.cpp \
    TestLightingEngine.cpp \
    ../src/LightingEngine.cpp \
    TestDeviceWriter.cpp \
    ../src/DeviceWriter.cpp \
    TestUledsBacklight.cpp \
    ../src/UledsBacklight.cpp \
    TestAccentColorSource.cpp \
    ../src/AccentColorSource.cpp \
    TestSystemSetup.cpp \
    TestStatusText.cpp \
    ../src/SystemSetup.cpp \
    ../src/PowerDevilProbe.cpp \
    ../src/StatusText.cpp \
    TestSettingsTab.cpp \
    ../src/SettingsTab.cpp
