#include "ReadingStatsStore.h"

#include <QSettings>

#include <algorithm>

namespace {

constexpr const char *kGroup = "ReadingStats";

struct Entry
{
    QString date; // Qt::ISODate ("yyyy-MM-dd")
    QString bookHash;
    int seconds = 0;
    int pages = 0;
};

QVector<Entry> readAll()
{
    QSettings settings;
    QVector<Entry> result;
    const int size = settings.beginReadArray(kGroup);
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        Entry e;
        e.date = settings.value("date").toString();
        e.bookHash = settings.value("bookHash").toString();
        e.seconds = settings.value("seconds", 0).toInt();
        e.pages = settings.value("pages", 0).toInt();
        if (!e.date.isEmpty() && !e.bookHash.isEmpty()) {
            result.append(e);
        }
    }
    settings.endArray();
    return result;
}

void writeAll(const QVector<Entry> &entries)
{
    QSettings settings;
    settings.remove(kGroup);
    settings.beginWriteArray(kGroup);
    for (int i = 0; i < entries.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("date", entries[i].date);
        settings.setValue("bookHash", entries[i].bookHash);
        settings.setValue("seconds", entries[i].seconds);
        settings.setValue("pages", entries[i].pages);
    }
    settings.endArray();
}

} // namespace

namespace ReadingStatsStore {

void recordSession(const QString &bookHash, const QDate &date, int secondsSpent, int pagesAdvanced)
{
    if (bookHash.isEmpty() || !date.isValid() || (secondsSpent <= 0 && pagesAdvanced <= 0)) {
        return;
    }

    QVector<Entry> entries = readAll();
    const QString dateStr = date.toString(Qt::ISODate);
    for (Entry &e : entries) {
        if (e.date == dateStr && e.bookHash == bookHash) {
            e.seconds += secondsSpent;
            e.pages += pagesAdvanced;
            writeAll(entries);
            return;
        }
    }
    entries.append({dateStr, bookHash, secondsSpent, pagesAdvanced});
    writeAll(entries);
}

QVector<QDate> activeDates()
{
    QVector<QDate> dates;
    for (const Entry &e : readAll()) {
        const QDate d = QDate::fromString(e.date, Qt::ISODate);
        if (d.isValid() && !dates.contains(d)) {
            dates.append(d);
        }
    }
    return dates;
}

int totalSecondsOn(const QDate &date)
{
    int total = 0;
    const QString dateStr = date.toString(Qt::ISODate);
    for (const Entry &e : readAll()) {
        if (e.date == dateStr) {
            total += e.seconds;
        }
    }
    return total;
}

int totalPagesOn(const QDate &date)
{
    int total = 0;
    const QString dateStr = date.toString(Qt::ISODate);
    for (const Entry &e : readAll()) {
        if (e.date == dateStr) {
            total += e.pages;
        }
    }
    return total;
}

int totalSecondsAllTime()
{
    int total = 0;
    for (const Entry &e : readAll()) {
        total += e.seconds;
    }
    return total;
}

int totalPagesAllTime()
{
    int total = 0;
    for (const Entry &e : readAll()) {
        total += e.pages;
    }
    return total;
}

int totalSecondsForBook(const QString &bookHash)
{
    int total = 0;
    for (const Entry &e : readAll()) {
        if (e.bookHash == bookHash) {
            total += e.seconds;
        }
    }
    return total;
}

int totalPagesForBook(const QString &bookHash)
{
    int total = 0;
    for (const Entry &e : readAll()) {
        if (e.bookHash == bookHash) {
            total += e.pages;
        }
    }
    return total;
}

int currentStreakDays()
{
    const QVector<QDate> dates = activeDates();
    if (dates.isEmpty()) {
        return 0;
    }

    QDate cursor = QDate::currentDate();
    if (!dates.contains(cursor)) {
        cursor = cursor.addDays(-1);
        if (!dates.contains(cursor)) {
            return 0;
        }
    }

    int streak = 0;
    while (dates.contains(cursor)) {
        ++streak;
        cursor = cursor.addDays(-1);
    }
    return streak;
}

int longestStreakDays()
{
    QVector<QDate> dates = activeDates();
    if (dates.isEmpty()) {
        return 0;
    }
    std::sort(dates.begin(), dates.end());

    int longest = 1;
    int current = 1;
    for (int i = 1; i < dates.size(); ++i) {
        if (dates[i - 1].daysTo(dates[i]) == 1) {
            ++current;
            longest = std::max(longest, current);
        } else {
            current = 1;
        }
    }
    return longest;
}

qreal averagePagesPerDay(int days)
{
    if (days <= 0) {
        return 0.0;
    }
    int total = 0;
    const QDate today = QDate::currentDate();
    for (int i = 0; i < days; ++i) {
        total += totalPagesOn(today.addDays(-i));
    }
    return qreal(total) / days;
}

} // namespace ReadingStatsStore
