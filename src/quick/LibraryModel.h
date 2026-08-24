#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QVector>

// QML-facing list model over app/RecentFiles.h, the same recently-opened
// list the desktop LibraryView (src/ui/LibraryView.cpp) shows — just a
// different presentation (GridView instead of QListWidget) of the same
// QSettings-backed data.
class LibraryModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        FilePathRole = Qt::UserRole + 1, // filesystem path on desktop, content:// URI on Android
        TitleRole,
        FormatRole,
        LastOpenedRole,
    };
    Q_ENUM(Roles)

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Re-reads app/RecentFiles.h's QSettings-backed list. Call after
    // recordOpened()/removeEntry() change the underlying store.
    Q_INVOKABLE void refresh();

    Q_INVOKABLE void recordOpened(const QString &filePath, const QString &title,
                                   const QString &format);
    Q_INVOKABLE void removeEntry(const QString &filePath);

private:
    struct Row
    {
        QString filePath;
        QString title;
        QString format;
        QDateTime lastOpened;
    };
    QVector<Row> m_rows;
};
