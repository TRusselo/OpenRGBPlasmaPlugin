#include "TestAccentColorSource.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "AccentColorSource.h"

namespace
{
QString writeFile(const QTemporaryDir& dir, const QByteArray& content)
{
    const QString temporary = dir.filePath(QStringLiteral("kdeglobals.new"));
    const QString target = dir.filePath(QStringLiteral("kdeglobals"));
    QFile file(temporary);
    if(file.open(QIODevice::WriteOnly))
    {
        file.write(content);
        file.close();
    }
    QFile::remove(target);
    QFile::rename(temporary, target);
    return target;
}
}

void TestAccentColorSource::customAccentWins()
{
    QTemporaryDir dir;
    const QString path = writeFile(dir, "[General]\nAccentColor=61,174,233\n\n[Colors:View]\nForegroundActive=1,2,3\n");
    QCOMPARE(AccentColorSource::readAccent(path), std::optional<Rgb>(makeRgb(61, 174, 233)));
}

void TestAccentColorSource::schemeColorIsTheFallback()
{
    QTemporaryDir dir;
    const QString path = writeFile(dir, "[General]\nColorScheme=BreezeDark\n\n[Colors:View]\nForegroundActive=1,2,3\n");
    QCOMPARE(AccentColorSource::readAccent(path), std::optional<Rgb>(makeRgb(1, 2, 3)));
}

void TestAccentColorSource::whiteWhenNoKeys()
{
    QTemporaryDir dir;
    const QString path = writeFile(dir, "[General]\nColorScheme=BreezeDark\n");
    QCOMPARE(AccentColorSource::readAccent(path), std::optional<Rgb>(makeRgb(255, 255, 255)));
}

void TestAccentColorSource::missingFileIsUnavailable()
{
    QTemporaryDir dir;
    QCOMPARE(AccentColorSource::readAccent(dir.filePath(QStringLiteral("nope"))), std::optional<Rgb>());
}

void TestAccentColorSource::parsesHexAndAlpha()
{
    QCOMPARE(AccentColorSource::parseColor(QStringLiteral("#3daee9")), std::optional<Rgb>(makeRgb(61, 174, 233)));
    QCOMPARE(AccentColorSource::parseColor(QStringLiteral("61,174,233,255")), std::optional<Rgb>(makeRgb(61, 174, 233)));
    QCOMPARE(AccentColorSource::parseColor(QStringLiteral(" 61, 174 ,233 ")), std::optional<Rgb>(makeRgb(61, 174, 233)));
}

void TestAccentColorSource::garbageIsIgnored()
{
    QCOMPARE(AccentColorSource::parseColor(QStringLiteral("red,green,blue")), std::optional<Rgb>());
    QCOMPARE(AccentColorSource::parseColor(QStringLiteral("300,1,1")), std::optional<Rgb>());
    QCOMPARE(AccentColorSource::parseColor(QStringLiteral("1,2")), std::optional<Rgb>());

    QTemporaryDir dir;
    const QString path = writeFile(dir, "no group line\n[General\nAccentColor=bogus\n=5\n[Colors:View]\nForegroundActive=10,20,30\n");
    QCOMPARE(AccentColorSource::readAccent(path), std::optional<Rgb>(makeRgb(10, 20, 30)));
}

void TestAccentColorSource::emitsWhenFileIsReplaced()
{
    QTemporaryDir dir;
    const QString path = writeFile(dir, "[General]\nAccentColor=1,1,1\n");
    AccentColorSource source(path);
    QSignalSpy spy(&source, &AccentColorSource::accentChanged);

    source.start();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toUInt(), makeRgb(1, 1, 1));

    writeFile(dir, "[General]\nAccentColor=200,10,10\n");
    QTRY_COMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toBool(), true);
    QCOMPARE(spy.at(1).at(1).toUInt(), makeRgb(200, 10, 10));
}
