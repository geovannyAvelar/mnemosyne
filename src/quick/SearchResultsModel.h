#pragma once

#include "core/ReaderView.h"

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QString>
#include <QVector>

// QML-facing list model over epub/EpubSearch.h's searchEpubFile(), the
// mobile analog of desktop's SearchDock + MainWindow's QtConcurrent-based
// wiring (see ui/MainWindow.cpp). Runs each search on a worker thread via
// QtConcurrent::run so a large book's chapter-by-chapter QTextDocument
// conversion doesn't block the UI thread; search() itself is what
// EpubReaderScreen.qml calls from a debounced search field, filePath is
// EpubReaderModel::filePath for whatever book is currently open.
class SearchResultsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool isSearching READ isSearching NOTIFY isSearchingChanged)

public:
    enum Roles {
        TargetIndexRole = Qt::UserRole + 1,
        LabelRole,
        SnippetRole,
    };
    Q_ENUM(Roles)

    explicit SearchResultsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool isSearching() const { return m_isSearching; }

    Q_INVOKABLE void search(const QString &filePath, const QString &query);
    Q_INVOKABLE void clear();

signals:
    void isSearchingChanged();

private:
    void applyResults();

    QVector<SearchResult> m_results;
    QFutureWatcher<QVector<SearchResult>> m_watcher;
    bool m_isSearching = false;

    // Bumped by both search() and clear() so a search that's still running
    // when the user erases the query (or fires a newer one) can tell, once
    // it finishes, that its result is stale and skip applying it -- the
    // watcher's own finished() signal fires unconditionally for whatever
    // future is currently set, so this is the only guard against that race.
    int m_generation = 0;
    int m_dispatchedGeneration = -1;
};
