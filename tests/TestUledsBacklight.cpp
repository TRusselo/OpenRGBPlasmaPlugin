#include "TestUledsBacklight.h"

#include <cstring>
#include <unistd.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "UledsBacklight.h"

void TestUledsBacklight::registrationMatchesKernelLayout()
{
    const QByteArray data = UledsBacklight::registration(QStringLiteral("openrgb::kbd_backlight"), 100);
    QCOMPARE(data.size(), 68);
    QCOMPARE(QByteArray(data.constData()), QByteArray("openrgb::kbd_backlight"));
    QCOMPARE(data.at(22), char(0));
    int maximum = 0;
    std::memcpy(&maximum, data.constData() + 64, sizeof(int));
    QCOMPARE(maximum, 100);
}

void TestUledsBacklight::registrationTruncatesLongNames()
{
    const QByteArray data = UledsBacklight::registration(QString(100, QLatin1Char('x')), 100);
    QCOMPARE(data.size(), 68);
    QCOMPARE(int(std::strlen(data.constData())), 63);
}

void TestUledsBacklight::parsesOneIntPerRead()
{
    int value = 42;
    const QByteArray data(reinterpret_cast<const char*>(&value), sizeof(int));
    QCOMPARE(UledsBacklight::parseLevel(data), std::optional<int>(42));
    QCOMPARE(UledsBacklight::parseLevel(QByteArray("abc")), std::optional<int>());
}

void TestUledsBacklight::openReportsMissingDevice()
{
    QTemporaryDir dir;
    UledsBacklight backlight(QStringLiteral("test::kbd_backlight"), dir.filePath(QStringLiteral("uleds")), dir.path());
    QCOMPARE(backlight.open(), UledsBacklight::Status::Missing);
}

void TestUledsBacklight::openReportsNoPermission()
{
    if(geteuid() == 0)
    {
        QSKIP("root can open any file");
    }
    QTemporaryDir dir;
    QFile device(dir.filePath(QStringLiteral("uleds")));
    QVERIFY(device.open(QIODevice::WriteOnly));
    device.close();
    QVERIFY(device.setPermissions(QFileDevice::Permissions()));
    UledsBacklight backlight(QStringLiteral("test::kbd_backlight"), device.fileName(), dir.path());
    QCOMPARE(backlight.open(), UledsBacklight::Status::NoPermission);
}

void TestUledsBacklight::openRefusesTakenName()
{
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("test::kbd_backlight")));
    UledsBacklight backlight(QStringLiteral("test::kbd_backlight"), dir.filePath(QStringLiteral("uleds")), dir.path());
    QCOMPARE(backlight.open(), UledsBacklight::Status::NameTaken);
}
