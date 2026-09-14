#pragma once

#include "core/ReaderView.h"
#include "epub/EpubSearch.h"

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QString>
#include <QVector>

// QML-facing list model over epub/EpubSearch.h's searchEpubFile(), the
// mobile analog of desktop's SearchDock + MainWindow's QtConcurrent-based
// wiring (see ui/MainWindow.cpp's m_epubSearchWatcher). Runs each search on
// a worker thread via QtConcurrent::run so a large book's chapter-by-
// chapter QTextDocument conversion doesn't block the UI thread; results
// are inserted into the model as each chapter match is found rather than
// only once the whole book has been scanned, and cancel() lets a search
// still in flight be aborted (e.g. because the user kept typing).
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

    // Starts a new search, first cancelling whatever search is still
    // running (see cancel()) so its stray results can't land after this
    // one's.
    Q_INVOKABLE void search(const QString &filePath, const QString &query);

    // Stops a search still in flight without touching results already
    // found -- the QML search field's Cancel button. A no-op if nothing is
    // running. The worker thread notices on its next per-chapter check
    // (see epub/EpubSearch.h), so isSearching stays true for a moment
    // after this returns.
    Q_INVOKABLE void cancel();

    Q_INVOKABLE void clear();

signals:
    void isSearchingChanged();

private:
    void appendResult(const SearchResult &result, int generation);
    void searchFinished();

    QVector<SearchResult> m_results;
    QFutureWatcher<void> m_watcher;
    EpubSearchCancelToken m_cancelToken;
    bool m_isSearching = false;

    // Bumped by search() and clear() so a result or finished() callback
    // from a search that's since been superseded (or explicitly cleared)
    // can tell it's stale and skip applying -- onResult crosses threads via
    // a queued QMetaObject::invokeMethod call that was already posted
    // before the newer search()/clear() ran, so it can't simply be undone.
    int m_generation = 0;
};
