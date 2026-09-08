#pragma once

#include <QDate>
#include <QString>
#include <QVector>

// Persists reading activity -- time spent and "pages" advanced -- per day
// per book, via QSettings. Backs the Stats screen/dock on both desktop and
// mobile. Nothing here starts/stops a session; see
// app/ReadingSessionTracker.h for the thing that actually calls
// recordSession() as the reader switches between books.
namespace ReadingStatsStore {

// Adds secondsSpent and pagesAdvanced to whatever's already recorded for
// bookHash on date -- accumulates rather than replaces, so re-opening the
// same book more than once in a day adds up correctly. "Pages" is
// PdfPageStackView/ComicView's literal page count for PDF/CBZ, but for
// EPUB/MOBI/Markdown/TXT it's really just IReaderView::currentPosition()'s
// delta (spine index, heading index, or character offset) -- see
// ReaderView.h's own doc comment for why that field means different things
// per format. Treated uniformly here as an approximate "reading pace"
// figure rather than a literal page count for those formats, the same
// loose reuse TocNode::pageNumber already gets elsewhere in this codebase.
void recordSession(const QString &bookHash, const QDate &date, int secondsSpent, int pagesAdvanced);

// Every date with at least one recorded session, in no particular order.
QVector<QDate> activeDates();

int totalSecondsOn(const QDate &date);
int totalPagesOn(const QDate &date);

int totalSecondsAllTime();
int totalPagesAllTime();

int totalSecondsForBook(const QString &bookHash);
int totalPagesForBook(const QString &bookHash);

// Consecutive days of activity ending today, or ending yesterday if today
// has no activity recorded yet -- so the streak isn't shown as already
// broken first thing in the morning before the reader has opened anything.
// 0 if neither today nor yesterday has activity.
int currentStreakDays();

// The longest run of consecutive active days ever recorded.
int longestStreakDays();

// Average pages/day over the last `days` calendar days (today inclusive).
// A day with no recorded activity counts as 0, not as excluded -- an
// average over active days only would flatter a reader who reads in rare
// bursts.
qreal averagePagesPerDay(int days = 30);

} // namespace ReadingStatsStore
