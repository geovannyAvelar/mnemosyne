#pragma once

#include "core/Highlight.h"

#include <QAbstractListModel>
#include <QString>
#include <QVector>

// QML-facing list model over app/HighlightStore.h, scoped to whichever
// document is currently open — same pattern as BookmarksModel (see that
// class for the reasoning behind the settable bookHash property).
class HighlightsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString bookHash READ bookHash WRITE setBookHash NOTIFY bookHashChanged)

public:
    enum Roles {
        TargetIndexRole = Qt::UserRole + 1,
        PageRectRole, // PDF only; null (all-zero) for EPUB, see core/Highlight.h
        TextRole,
        CreatedAtRole,
    };
    Q_ENUM(Roles)

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString bookHash() const { return m_bookHash; }
    void setBookHash(const QString &bookHash);

    // Highlights whose targetIndex == targetIndex (current PDF page/EPUB
    // spine index), for QML to render as an overlay on that page/chapter.
    Q_INVOKABLE QVariantList highlightsForTarget(int targetIndex) const;

    // pageRect: page-space bounding rect of the selection (PDF), or a
    // null/default QRectF for EPUB (text-based relocation only, matching
    // desktop's existing EPUB highlight model — see core/Highlight.h).
    Q_INVOKABLE void addHighlight(int targetIndex, const QRectF &pageRect, const QString &text);
    Q_INVOKABLE void removeHighlightAt(int row);

signals:
    void bookHashChanged();

private:
    void refresh();

    QString m_bookHash;
    QVector<Highlight> m_highlights;
};
