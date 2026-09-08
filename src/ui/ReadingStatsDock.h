#pragma once

#include <QDockWidget>

class QLabel;

// Sidebar tab showing whole-library reading stats (see
// app/ReadingStatsStore.h): time spent and pages read today, the reader's
// current/longest day-streak, their average pages/day pace, and all-time
// totals. Unlike BookInfoDock/FormFieldsDock, this never hides itself --
// it's not tied to whatever the current tab happens to be, so it stays
// available (and just as relevant) even with no document open at all.
class ReadingStatsDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit ReadingStatsDock(QWidget *parent = nullptr);

    // Re-reads every figure from ReadingStatsStore. Call whenever a
    // ReadingSessionTracker::stop() call may have just changed one (see
    // MainWindow's own calls to both), and once at startup.
    void refresh();

private:
    QLabel *m_todayLabel;
    QLabel *m_streakLabel;
    QLabel *m_paceLabel;
    QLabel *m_allTimeLabel;
};
