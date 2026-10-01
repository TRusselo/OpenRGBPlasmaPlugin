#pragma once

#include <QObject>

class TestSystemSetup : public QObject
{
    Q_OBJECT

private slots:
    void readyWhenDeviceIsAccessible();
    void needsSetupWhenDeviceIsNotAccessible();
    void needsSetupWhenModuleIsAvailable();
    void needsSetupWhenModuleIsBuiltIn();
    void noKernelSupportOtherwise();
    void setupScriptWritesBothFilesAndLoadsModule();
    void powerDevilRestartRule();
};
