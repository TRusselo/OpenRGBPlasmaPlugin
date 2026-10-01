#include <memory>
#include <vector>

#include <QApplication>
#include <QTest>

#include "TestPluginMetadata.h"
#include "TestDimming.h"
#include "TestDeviceKey.h"
#include "TestPluginSettings.h"
#include "TestLightingEngine.h"

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    std::vector<char*> args(argv, argv + argc);
    QString filter;
    if(args.size() > 1 && args[1][0] != '-')
    {
        filter = QString::fromLocal8Bit(args[1]);
        args.erase(args.begin() + 1);
    }

    std::vector<std::unique_ptr<QObject>> tests;
    tests.emplace_back(new TestPluginMetadata);
    tests.emplace_back(new TestDimming);
    tests.emplace_back(new TestDeviceKey);
    tests.emplace_back(new TestPluginSettings);
    tests.emplace_back(new TestLightingEngine);

    int status = 0;
    for(const std::unique_ptr<QObject>& test : tests)
    {
        if(filter.isEmpty() || filter == QLatin1String(test->metaObject()->className()))
        {
            status |= QTest::qExec(test.get(), int(args.size()), args.data());
        }
    }
    return status;
}
