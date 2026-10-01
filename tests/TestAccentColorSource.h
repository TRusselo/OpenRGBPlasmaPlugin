#pragma once

#include <QObject>

class TestAccentColorSource : public QObject
{
    Q_OBJECT

private slots:
    void customAccentWins();
    void schemeColorIsTheFallback();
    void whiteWhenNoKeys();
    void missingFileIsUnavailable();
    void parsesHexAndAlpha();
    void garbageIsIgnored();
    void emitsWhenFileIsReplaced();
};
