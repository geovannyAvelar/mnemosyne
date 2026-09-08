#include "ReadingStatsDock.h"

#include "app/ReadingStatsStore.h"

#include <QDate>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

namespace {

// "2h 5m", "45m", or "0m" -- never bothers with seconds, since a reading
// session's own granularity (whole tab-switches) makes second-level
// precision meaningless to show the reader.
QString formatDuration(int totalSeconds)
{
    const int minutes = totalSeconds / 60;
    const int hours = minutes / 60;
    const int remainingMinutes = minutes % 60;
    if (hours > 0) {
        return QObject::tr("%1h %2m").arg(hours).arg(remainingMinutes);
    }
    return QObject::tr("%1m").arg(remainingMinutes);
}

QString formatDays(int days)
{
    return days == 1 ? QObject::tr("1 day") : QObject::tr("%1 days").arg(days);
}

} // namespace

ReadingStatsDock::ReadingStatsDock(QWidget *parent)
    : QDockWidget(tr("Stats"), parent)
{
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(14);

    auto addSection = [&](const QString &title) {
        auto *titleLabel = new QLabel(title, container);
        QFont font = titleLabel->font();
        font.setBold(true);
        font.setPointSizeF(font.pointSizeF() - 1.0);
        titleLabel->setFont(font);
        titleLabel->setStyleSheet(QStringLiteral("color: palette(mid); letter-spacing: 1px;"));
        layout->addWidget(titleLabel);
    };

    addSection(tr("TODAY"));
    m_todayLabel = new QLabel(container);
    m_todayLabel->setWordWrap(true);
    layout->addWidget(m_todayLabel);

    addSection(tr("STREAK"));
    m_streakLabel = new QLabel(container);
    m_streakLabel->setWordWrap(true);
    layout->addWidget(m_streakLabel);

    addSection(tr("PACE"));
    m_paceLabel = new QLabel(container);
    m_paceLabel->setWordWrap(true);
    layout->addWidget(m_paceLabel);

    addSection(tr("ALL TIME"));
    m_allTimeLabel = new QLabel(container);
    m_allTimeLabel->setWordWrap(true);
    layout->addWidget(m_allTimeLabel);

    layout->addStretch(1);
    setWidget(container);

    refresh();
}

void ReadingStatsDock::refresh()
{
    const QDate today = QDate::currentDate();
    m_todayLabel->setText(tr("%1 · %2 pages")
                               .arg(formatDuration(ReadingStatsStore::totalSecondsOn(today)))
                               .arg(ReadingStatsStore::totalPagesOn(today)));

    const int current = ReadingStatsStore::currentStreakDays();
    const int longest = ReadingStatsStore::longestStreakDays();
    m_streakLabel->setText(current > 0 ? tr("%1 current · %2 best").arg(formatDays(current), formatDays(longest))
                                       : tr("No current streak · %1 best").arg(formatDays(longest)));

    m_paceLabel->setText(tr("%1 pages/day, last 30 days").arg(ReadingStatsStore::averagePagesPerDay(30), 0, 'f', 1));

    m_allTimeLabel->setText(tr("%1 · %2 pages")
                                 .arg(formatDuration(ReadingStatsStore::totalSecondsAllTime()))
                                 .arg(ReadingStatsStore::totalPagesAllTime()));
}
