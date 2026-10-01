#include "SystemSetup.h"

#include <unistd.h>

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSysInfo>

namespace
{
bool kernelHasModule(const QString& modulesDir)
{
    QFile builtin(modulesDir + QStringLiteral("/modules.builtin"));
    if(builtin.open(QIODevice::ReadOnly | QIODevice::Text) && builtin.readAll().contains("/uleds.ko"))
    {
        return true;
    }
    QDirIterator modules(modulesDir + QStringLiteral("/kernel/drivers/leds"), {QStringLiteral("uleds.ko*")}, QDir::Files);
    return modules.hasNext();
}
}

SystemSetup::Paths SystemSetup::systemPaths()
{
    return Paths{QStringLiteral("/dev/uleds"), QStringLiteral("/sys/module/uleds"), QStringLiteral("/lib/modules/") + QSysInfo::kernelVersion()};
}

SystemSetup::State SystemSetup::detect(const Paths& paths)
{
    if(QFileInfo::exists(paths.device))
    {
        return ::access(QFile::encodeName(paths.device).constData(), R_OK | W_OK) == 0 ? State::Ready : State::NeedsSetup;
    }
    if(QFileInfo::exists(paths.loadedModule) || kernelHasModule(paths.modulesDir))
    {
        return State::NeedsSetup;
    }
    return State::NoKernelSupport;
}

QString SystemSetup::setupScript()
{
    return QStringLiteral(
        "echo uleds > /etc/modules-load.d/openrgb-kbd-backlight.conf && "
        "echo 'KERNEL==\"uleds\", TAG+=\"uaccess\"' > /etc/udev/rules.d/70-openrgb-kbd-backlight.rules && "
        "modprobe uleds && "
        "udevadm control --reload && "
        "udevadm trigger --name-match=uleds && "
        "udevadm settle");
}

QString SystemSetup::manualCommands()
{
    return QStringLiteral(
        "echo uleds | sudo tee /etc/modules-load.d/openrgb-kbd-backlight.conf\n"
        "echo 'KERNEL==\"uleds\", TAG+=\"uaccess\"' | sudo tee /etc/udev/rules.d/70-openrgb-kbd-backlight.rules\n"
        "sudo modprobe uleds\n"
        "sudo udevadm control --reload\n"
        "sudo udevadm trigger --name-match=uleds");
}

void SystemSetup::runSetup()
{
    auto* process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        const bool success = exitStatus == QProcess::NormalExit && exitCode == 0;
        emit setupFinished(success, success ? QString() : tr("Setup did not complete (exit code %1).").arg(exitCode));
        process->deleteLater();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if(error == QProcess::FailedToStart)
        {
            emit setupFinished(false, tr("pkexec is not available."));
            process->deleteLater();
        }
    });
    process->start(QStringLiteral("pkexec"), {QStringLiteral("/bin/sh"), QStringLiteral("-c"), setupScript()});
}
