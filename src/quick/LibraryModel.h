#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QVector>

// QML-facing list model over app/RecentFiles.h, the same recently-opened
// list the desktop LibraryView (src/ui/LibraryView.cpp) shows — just a
// different presentation (GridView instead of QListWidget) of the same
// QSettings-backed data. Also bridges app/CollectionStore.h and
// app/TagStore.h for shelves/tags, mirroring how LibraryView's own context
// menu uses them directly -- kept on this one model rather than two
// separate CollectionsModel/TagsModel classes since QML only ever needs
// simple CRUD calls here, not a second list model of their own.
class LibraryModel : public QAbstractListModel
{
    Q_OBJECT
    // Filters rows to just entries whose contentHash is a member of this
    // collection/carries this tag; empty means "no filter" for that axis.
    // The two combine (AND) when both are set. Setting either re-runs
    // refresh() immediately.
    Q_PROPERTY(QString collectionFilter READ collectionFilter WRITE setCollectionFilter NOTIFY collectionFilterChanged)
    Q_PROPERTY(QString tagFilter READ tagFilter WRITE setTagFilter NOTIFY tagFilterChanged)

public:
    enum Roles {
        FilePathRole = Qt::UserRole + 1, // filesystem path on desktop, content:// URI on Android
        TitleRole,
        FormatRole,
        LastOpenedRole,
        ContentHashRole, // empty for a pre-hash Recent entry -- see RecentFiles::Entry's own doc comment
    };
    Q_ENUM(Roles)

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString collectionFilter() const { return m_collectionFilter; }
    void setCollectionFilter(const QString &collection);
    QString tagFilter() const { return m_tagFilter; }
    void setTagFilter(const QString &tag);

    // Re-reads app/RecentFiles.h's QSettings-backed list (filtered by
    // collectionFilter/tagFilter, if set). Call after recordOpened()/
    // removeEntry() change the underlying store, or after a shelf/tag
    // CRUD call below that should affect which rows currently match.
    Q_INVOKABLE void refresh();

    Q_INVOKABLE void recordOpened(const QString &filePath, const QString &title,
                                   const QString &format);
    Q_INVOKABLE void removeEntry(const QString &filePath);

    // CollectionStore bridge -- see app/CollectionStore.h for each of
    // these' own behavior/doc comments; identical signatures, just exposed
    // to QML.
    Q_INVOKABLE QStringList allCollections() const;
    Q_INVOKABLE QStringList collectionsForBook(const QString &bookHash) const;
    // Returns the (trimmed) name, or empty if name was blank -- lets QML
    // know whether to also select the newly created shelf.
    Q_INVOKABLE QString createCollection(const QString &name);
    Q_INVOKABLE void addBookToCollection(const QString &bookHash, const QString &collection);
    Q_INVOKABLE void removeBookFromCollection(const QString &bookHash, const QString &collection);
    Q_INVOKABLE void deleteCollection(const QString &collection);

    // TagStore bridge.
    Q_INVOKABLE QStringList allTags() const;
    Q_INVOKABLE QStringList tagsForBook(const QString &bookHash) const;
    Q_INVOKABLE void setTagsForBook(const QString &bookHash, const QStringList &tags);

signals:
    void collectionFilterChanged();
    void tagFilterChanged();

private:
    struct Row
    {
        QString filePath;
        QString title;
        QString format;
        QDateTime lastOpened;
        QString contentHash;
    };
    QVector<Row> m_rows;
    QString m_collectionFilter;
    QString m_tagFilter;
};
