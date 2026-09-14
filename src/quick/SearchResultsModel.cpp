#include "SearchResultsModel.h"

#include "epub/EpubSearch.h"

#include <QtConcurrentRun>

SearchResultsModel::SearchResultsModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(&m_watcher, &QFutureWatcher<QVector<SearchResult>>::finished, this, &SearchResultsModel::applyResults);
}

int SearchResultsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_results.size();
}

QVariant SearchResultsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_results.size()) {
        return {};
    }

    const SearchResult &result = m_results.at(index.row());
    switch (role) {
    case TargetIndexRole:
        return result.targetIndex;
    case LabelRole:
        return result.label;
    case SnippetRole:
        return result.snippet;
    default:
        return {};
    }
}

QHash<int, QByteArray> SearchResultsModel::roleNames() const
{
    return {
        {TargetIndexRole, "targetIndex"},
        {LabelRole, "label"},
        {SnippetRole, "snippet"},
    };
}

void SearchResultsModel::search(const QString &filePath, const QString &query)
{
    ++m_generation;

    if (filePath.isEmpty() || query.trimmed().isEmpty()) {
        clear();
        return;
    }

    m_isSearching = true;
    emit isSearchingChanged();

    // setFuture() on an already-running watcher detaches it from whatever
    // earlier search is still in flight -- that older QtConcurrent::run
    // keeps executing on the thread pool, but its result is simply never
    // delivered through this watcher, so a newer search's result can't be
    // clobbered by an older one finishing later. Same pattern desktop's
    // MainWindow.cpp uses its own QFutureWatcher for.
    m_dispatchedGeneration = m_generation;
    m_watcher.setFuture(QtConcurrent::run(searchEpubFile, filePath, query));
}

void SearchResultsModel::clear()
{
    ++m_generation; // invalidates applyResults() for any still-running search

    if (m_isSearching) {
        m_isSearching = false;
        emit isSearchingChanged();
    }
    if (m_results.isEmpty()) {
        return;
    }
    beginResetModel();
    m_results.clear();
    endResetModel();
}

void SearchResultsModel::applyResults()
{
    const bool isStale = m_dispatchedGeneration != m_generation;

    if (!isStale) {
        beginResetModel();
        m_results = m_watcher.result();
        endResetModel();
    }

    if (!isStale && m_isSearching) {
        m_isSearching = false;
        emit isSearchingChanged();
    }
}
