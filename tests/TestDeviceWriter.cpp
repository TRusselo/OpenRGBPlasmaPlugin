#include "TestDeviceWriter.h"

#include <memory>

#include <QSignalSpy>
#include <QTest>

#include "DeviceWriter.h"
#include "FakeLight.h"
#include "LightBuilders.h"

void TestDeviceWriter::coalescesWritesPerLight()
{
    auto light = std::make_shared<FakeLight>("a", directLight({makeRgb(1, 1, 1)}));
    DeviceWriter writer(30);
    writer.setLights({light});
    QSignalSpy spy(&writer, &DeviceWriter::written);

    writer.enqueue(QStringLiteral("a"), directLight({makeRgb(2, 2, 2)}), 1, false);
    writer.enqueue(QStringLiteral("a"), directLight({makeRgb(3, 3, 3)}), 2, false);

    QTRY_COMPARE(spy.count(), 1);
    QTest::qWait(60);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(light->applyCount, 1);
    QCOMPARE(light->state, directLight({makeRgb(3, 3, 3)}));
    QCOMPARE(spy.at(0).at(1).toULongLong(), quint64(2));
}

void TestDeviceWriter::writesEachLight()
{
    auto first = std::make_shared<FakeLight>("a", directLight({makeRgb(1, 1, 1)}));
    auto second = std::make_shared<FakeLight>("b", directLight({makeRgb(1, 1, 1)}));
    DeviceWriter writer(30);
    writer.setLights({first, second});
    QSignalSpy spy(&writer, &DeviceWriter::written);

    writer.enqueue(QStringLiteral("a"), directLight({makeRgb(4, 4, 4)}), 1, false);
    writer.enqueue(QStringLiteral("b"), directLight({makeRgb(5, 5, 5)}), 2, false);

    QTRY_COMPARE(spy.count(), 2);
    QCOMPARE(first->state, directLight({makeRgb(4, 4, 4)}));
    QCOMPARE(second->state, directLight({makeRgb(5, 5, 5)}));
}

void TestDeviceWriter::waitsForInterval()
{
    auto light = std::make_shared<FakeLight>("a", directLight({makeRgb(1, 1, 1)}));
    DeviceWriter writer(30);
    writer.setLights({light});

    writer.enqueue(QStringLiteral("a"), directLight({makeRgb(6, 6, 6)}), 1, false);

    QCOMPARE(light->applyCount, 0);
    QTRY_COMPARE(light->applyCount, 1);
}

void TestDeviceWriter::dropsWritesForRemovedLights()
{
    auto light = std::make_shared<FakeLight>("a", directLight({makeRgb(1, 1, 1)}));
    DeviceWriter writer(30);
    writer.setLights({light});
    QSignalSpy spy(&writer, &DeviceWriter::written);

    writer.enqueue(QStringLiteral("a"), directLight({makeRgb(7, 7, 7)}), 1, false);
    writer.setLights({});

    QTest::qWait(100);
    QCOMPARE(spy.count(), 0);
    QCOMPARE(light->applyCount, 0);
}

void TestDeviceWriter::coalescedWriteKeepsRestoreMode()
{
    DeviceWriter writer;
    auto light = std::make_shared<FakeLight>("a", directLight({makeRgb(1, 1, 1)}));
    writer.setLights({light});
    QSignalSpy spy(&writer, &DeviceWriter::written);

    writer.enqueue(QStringLiteral("a"), directLight({makeRgb(2, 2, 2)}), 1, true);
    writer.enqueue(QStringLiteral("a"), directLight({makeRgb(3, 3, 3)}), 2, false);

    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(light->state, directLight({makeRgb(3, 3, 3)}));
    QCOMPARE(light->lastRestoreMode, true);
}
