#include "HighlightsModel.h"

#include "app/HighlightStore.h"

#include <QDateTime>
#include <QVariantMap>

int HighlightsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_highlights.size();
}

QVariant HighlightsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_highlights.size()) {
        return {};
    }

    const Highlight &highlight = m_highlights.at(index.row());
    switch (role) {
    case TargetIndexRole:
        return highlight.targetIndex;
    case PageRectRole:
        return highlight.pageRect;
    case TextRole:
        return highlight.text;
    case CreatedAtRole:
        return highlight.createdAt;
    case NoteRole:
        return highlight.note;
    case ColorRole:
        return highlight.color;
    default:
        return {};
    }
}

QHash<int, QByteArray> HighlightsModel::roleNames() const
{
    return {
        {TargetIndexRole, "targetIndex"},
        {PageRectRole, "pageRect"},
        {TextRole, "text"},
        {CreatedAtRole, "createdAt"},
        {NoteRole, "note"},
        {ColorRole, "color"},
    };
}

void HighlightsModel::setBookHash(const QString &bookHash)
{
    if (m_bookHash == bookHash) {
        return;
    }
    m_bookHash = bookHash;
    emit bookHashChanged();
    refresh();
}

QVariantList HighlightsModel::highlightsForTarget(int targetIndex) const
{
    QVariantList result;
    for (int i = 0; i < m_highlights.size(); ++i) {
        const Highlight &highlight = m_highlights.at(i);
        if (highlight.targetIndex != targetIndex) {
            continue;
        }
        QVariantMap entry;
        entry["row"] = i; // for removeHighlightAt
        entry["pageRect"] = highlight.pageRect;
        entry["text"] = highlight.text;
        result.append(entry);
    }
    return result;
}

void HighlightsModel::addHighlight(int targetIndex, const QRectF &pageRect, const QString &text)
{
    if (m_bookHash.isEmpty() || text.isEmpty()) {
        return;
    }

    Highlight highlight;
    highlight.targetIndex = targetIndex;
    highlight.pageRect = pageRect;
    highlight.text = text;
    highlight.createdAt = QDateTime::currentDateTime();
    HighlightStore::addHighlight(m_bookHash, highlight);
    refresh();
}

void HighlightsModel::removeHighlightAt(int row)
{
    if (m_bookHash.isEmpty() || row < 0 || row >= m_highlights.size()) {
        return;
    }
    HighlightStore::removeHighlight(m_bookHash, row);
    refresh();
}

void HighlightsModel::refresh()
{
    beginResetModel();
    m_highlights = m_bookHash.isEmpty() ? QVector<Highlight>() : HighlightStore::highlightsFor(m_bookHash);
    endResetModel();
}
