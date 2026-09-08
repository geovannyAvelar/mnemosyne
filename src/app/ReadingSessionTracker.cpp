#include "ReadingSessionTracker.h"

#include "ReadingStatsStore.h"

#include <algorithm>

void ReadingSessionTracker::start(const QString &bookHash, int position)
{
    m_bookHash = bookHash;
    m_startedAt = QDateTime::currentDateTime();
    m_startPosition = position;
}

void ReadingSessionTracker::stop(int position)
{
    if (m_bookHash.isEmpty()) {
        return;
    }
    const int seconds = static_cast<int>(m_startedAt.secsTo(QDateTime::currentDateTime()));
    const int pages = std::max(0, position - m_startPosition);
    ReadingStatsStore::recordSession(m_bookHash, m_startedAt.date(), seconds, pages);
    m_bookHash.clear();
}
