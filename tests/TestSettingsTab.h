#pragma once

#include <QObject>

class TestSettingsTab : public QObject
{
    Q_OBJECT

private slots:
    void rowsReflectSettings();
    void togglingDimEmitsOptions();
    void fillingRowsEmitsNothing();
    void accentCheckboxEmits();
    void statusAndButtons();
};
