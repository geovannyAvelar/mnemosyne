#pragma once

#include "core/Bookmark.h"

#include <QAbstractListModel>
#include <QString>
#include <QVector>

// QML-facing list model over app/BookmarkStore.h, the same store the
// desktop BookmarksDock uses — just scoped to whichever document is
// currently open (see bookHash, set by the reader screen from
// PdfDocumentModel::bookHash / EpubReaderModel::bookHash) rather than
// desktop's per-tab MainWindow::m_currentFilePath tracking.
class BookmarksModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString bookHash READ bookHash WRITE setBookHash NOTIFY bookHashChanged)

public:
    enum Roles {
        TargetIndexRole = Qt::UserRole + 1,
        LabelRole,
        CreatedAtRole,
    };
    Q_ENUM(Roles)

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString bookHash() const { return m_bookHash; }
    void setBookHash(const QString &bookHash);

    // True if some bookmark already targets targetIndex (current page/spine
    // index) — used to drive a bookmark-toggle button's checked state.
    Q_INVOKABLE bool isBookmarked(int targetIndex) const;

    // Adds one if targetIndex isn't already bookmarked, else removes it.
    Q_INVOKABLE void toggleBookmark(int targetIndex, const QString &label);
    Q_INVOKABLE void removeBookmarkAt(int row);

signals:
    void bookHashChanged();

private:
    void refresh();

    QString m_bookHash;
    QVector<Bookmark> m_bookmarks;
};
