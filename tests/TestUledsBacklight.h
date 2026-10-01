#pragma once

#include <QObject>

class TestUledsBacklight : public QObject
{
    Q_OBJECT

private slots:
    void registrationMatchesKernelLayout();
    void registrationTruncatesLongNames();
    void parsesOneIntPerRead();
    void openReportsMissingDevice();
    void openReportsNoPermission();
    void openRefusesTakenName();
};
