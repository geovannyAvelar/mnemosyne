#include "ReadingStatsModel.h"

#include "app/ReadingStatsStore.h"

#include <QDate>

int ReadingStatsModel::todaySeconds() const
{
    return ReadingStatsStore::totalSecondsOn(QDate::currentDate());
}

int ReadingStatsModel::todayPages() const
{
    return ReadingStatsStore::totalPagesOn(QDate::currentDate());
}

int ReadingStatsModel::currentStreakDays() const
{
    return ReadingStatsStore::currentStreakDays();
}

int ReadingStatsModel::longestStreakDays() const
{
    return ReadingStatsStore::longestStreakDays();
}

double ReadingStatsModel::averagePagesPerDay() const
{
    return ReadingStatsStore::averagePagesPerDay(30);
}

int ReadingStatsModel::allTimeSeconds() const
{
    return ReadingStatsStore::totalSecondsAllTime();
}

int ReadingStatsModel::allTimePages() const
{
    return ReadingStatsStore::totalPagesAllTime();
}
