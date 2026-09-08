#pragma once

#include <QObject>

// QML-facing bridge over app/ReadingStatsStore.h -- plain Q_INVOKABLE
// pass-throughs, no stored state of its own (unlike LibraryModel/
// HighlightsModel, there's no list to keep in sync, just numbers to
// re-query whenever StatsScreen.qml is shown). See app/ReadingSessionTracker.h
// (exposed to QML separately, as the "readingSessionTracker" context
// property) for what actually populates this data as the reader switches
// between books.
class ReadingStatsModel : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    Q_INVOKABLE int todaySeconds() const;
    Q_INVOKABLE int todayPages() const;
    Q_INVOKABLE int currentStreakDays() const;
    Q_INVOKABLE int longestStreakDays() const;
    // Pages/day averaged over the last 30 days.
    Q_INVOKABLE double averagePagesPerDay() const;
    Q_INVOKABLE int allTimeSeconds() const;
    Q_INVOKABLE int allTimePages() const;
};
