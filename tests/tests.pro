include(../common.pri)

TEMPLATE = app
TARGET = plasma-integration-tests
CONFIG += console
CONFIG -= app_bundle
QT += core gui widgets dbus testlib

DEFINES += PLUGIN_LIBRARY_PATH=\\\"$$OUT_PWD/../src/libOpenRGBPlasmaPlugin.so\\\"

HEADERS += \
    TestPluginMetadata.h

SOURCES += \
    main.cpp \
    TestPluginMetadata.cpp
