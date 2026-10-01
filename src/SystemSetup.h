#pragma once

#include <QObject>
#include <QString>

class SystemSetup : public QObject
{
    Q_OBJECT

public:
    enum class State
    {
        Ready,
        NeedsSetup,
        NoKernelSupport,
    };

    struct Paths
    {
        QString device;
        QString loadedModule;
        QString modulesDir;
    };

    using QObject::QObject;

    static Paths systemPaths();
    static State detect(const Paths& paths);
    static QString setupScript();
    static QString manualCommands();

public slots:
    void runSetup();

signals:
    void setupFinished(bool success, const QString& message);
};
