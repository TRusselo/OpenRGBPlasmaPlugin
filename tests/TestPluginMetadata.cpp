#include "TestPluginMetadata.h"

#include <QJsonObject>
#include <QPluginLoader>
#include <QTest>

void TestPluginMetadata::declaresOpenRGBApiFive()
{
    QPluginLoader loader(QStringLiteral(PLUGIN_LIBRARY_PATH));
    const QJsonObject root = loader.metaData();
    const QJsonObject metadata = root.value(QStringLiteral("MetaData")).toObject();

    QCOMPARE(root.value(QStringLiteral("IID")).toString(), QStringLiteral("org.openrgb.OpenRGBPluginInterface"));
    QCOMPARE(metadata.value(QStringLiteral("OpenRGBPluginAPIVersion")).toInt(), 5);
    QCOMPARE(metadata.value(QStringLiteral("Id")).toString(), QStringLiteral("io.github.trusselo.openrgbplasmaplugin"));
    QCOMPARE(metadata.value(QStringLiteral("Name")).toString(), QStringLiteral("Plasma Integration"));
}
