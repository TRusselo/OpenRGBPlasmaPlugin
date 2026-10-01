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
    PlasmaIntegrationPlugin.h

SOURCES += \
    PlasmaIntegrationPlugin.cpp

DISTFILES += \
    PlasmaIntegrationPlugin.json
