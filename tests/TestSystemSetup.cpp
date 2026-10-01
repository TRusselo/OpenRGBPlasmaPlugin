#include "TestSystemSetup.h"

#include <unistd.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include "PowerDevilProbe.h"
#include "SystemSetup.h"

namespace
{
SystemSetup::Paths pathsIn(const QTemporaryDir& dir)
{
    return SystemSetup::Paths{dir.filePath(QStringLiteral("uleds")), dir.filePath(QStringLiteral("module/uleds")), dir.filePath(QStringLiteral("modules"))};
}

void touch(const QString& path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if(file.open(QIODevice::WriteOnly))
    {
        file.write("uleds\n");
    }
}
}

void TestSystemSetup::readyWhenDeviceIsAccessible()
{
    QTemporaryDir dir;
    touch(dir.filePath(QStringLiteral("uleds")));
    QCOMPARE(SystemSetup::detect(pathsIn(dir)), SystemSetup::State::Ready);
}

void TestSystemSetup::needsSetupWhenDeviceIsNotAccessible()
{
    if(geteuid() == 0)
    {
        QSKIP("root can open any file");
    }
    QTemporaryDir dir;
    touch(dir.filePath(QStringLiteral("uleds")));
    QVERIFY(QFile(dir.filePath(QStringLiteral("uleds"))).setPermissions(QFileDevice::Permissions()));
    QCOMPARE(SystemSetup::detect(pathsIn(dir)), SystemSetup::State::NeedsSetup);
}

void TestSystemSetup::needsSetupWhenModuleIsAvailable()
{
    QTemporaryDir dir;
    touch(dir.filePath(QStringLiteral("modules/kernel/drivers/leds/uleds.ko.zst")));
    QCOMPARE(SystemSetup::detect(pathsIn(dir)), SystemSetup::State::NeedsSetup);
}

void TestSystemSetup::needsSetupWhenModuleIsBuiltIn()
{
    QTemporaryDir dir;
    QDir().mkpath(dir.filePath(QStringLiteral("modules")));
    QFile builtin(dir.filePath(QStringLiteral("modules/modules.builtin")));
    QVERIFY(builtin.open(QIODevice::WriteOnly));
    builtin.write("kernel/drivers/leds/led-class.ko\nkernel/drivers/leds/uleds.ko\n");
    builtin.close();
    QCOMPARE(SystemSetup::detect(pathsIn(dir)), SystemSetup::State::NeedsSetup);
}

void TestSystemSetup::noKernelSupportOtherwise()
{
    QTemporaryDir dir;
    QDir().mkpath(dir.filePath(QStringLiteral("modules/kernel/drivers/leds")));
    QCOMPARE(SystemSetup::detect(pathsIn(dir)), SystemSetup::State::NoKernelSupport);
}

void TestSystemSetup::setupScriptWritesBothFilesAndLoadsModule()
{
    const QString script = SystemSetup::setupScript();
    QVERIFY(script.contains(QStringLiteral("echo uleds > /etc/modules-load.d/openrgb-kbd-backlight.conf")));
    QVERIFY(script.contains(QStringLiteral("echo 'KERNEL==\"uleds\", TAG+=\"uaccess\"' > /etc/udev/rules.d/70-openrgb-kbd-backlight.rules")));
    QVERIFY(script.contains(QStringLiteral("modprobe uleds")));
    QVERIFY(script.contains(QStringLiteral("udevadm trigger --name-match=uleds")));
    QVERIFY(SystemSetup::manualCommands().contains(QStringLiteral("sudo modprobe uleds")));
}

void TestSystemSetup::powerDevilRestartRule()
{
    QCOMPARE(PowerDevilProbe::needsRestart(true, true, true, 100), false);
    QCOMPARE(PowerDevilProbe::needsRestart(true, true, true, 0), true);
    QCOMPARE(PowerDevilProbe::needsRestart(true, true, false, 0), true);
    QCOMPARE(PowerDevilProbe::needsRestart(false, true, false, 0), false);
    QCOMPARE(PowerDevilProbe::needsRestart(true, false, false, 0), false);
}
