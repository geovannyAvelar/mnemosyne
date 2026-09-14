#include "SearchResultsModel.h"

#include <QtConcurrentRun>

SearchResultsModel::SearchResultsModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(&m_watcher, &QFutureWatcher<void>::finished, this, &SearchResultsModel::searchFinished);
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
    cancel();
    ++m_generation;

    if (!m_results.isEmpty()) {
        beginResetModel();
        m_results.clear();
        endResetModel();
    }

    if (filePath.isEmpty() || query.trimmed().isEmpty()) {
        return;
    }

    m_isSearching = true;
    emit isSearchingChanged();

    m_cancelToken = makeSearchCancelToken();
    const int generation = m_generation;
    const EpubSearchCancelToken cancelToken = m_cancelToken;

    // Runs on the QtConcurrent thread pool; each onResult call happens on
    // that worker thread too, so it hops back to this model's own (GUI)
    // thread via invokeMethod before touching m_results. The generation
    // captured here, not read from m_generation again, is what lets
    // appendResult() recognize a result from a search this model has since
    // moved on from.
    auto onResult = [this, generation](const SearchResult &result) {
        QMetaObject::invokeMethod(
            this, [this, result, generation] { appendResult(result, generation); }, Qt::QueuedConnection);
    };

    m_watcher.setFuture(QtConcurrent::run(searchEpubFile, filePath, query, onResult, cancelToken));
}

void SearchResultsModel::cancel()
{
    if (m_cancelToken) {
        m_cancelToken->store(true);
    }
}

void SearchResultsModel::clear()
{
    cancel();
    ++m_generation;

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

void SearchResultsModel::appendResult(const SearchResult &result, int generation)
{
    if (generation != m_generation) {
        return; // superseded by a newer search() or clear() since this result was found
    }
    const int row = m_results.size();
    beginInsertRows(QModelIndex(), row, row);
    m_results.append(result);
    endInsertRows();
}

void SearchResultsModel::searchFinished()
{
    if (!m_isSearching) {
        return; // already stopped by clear(); this is the now-stale search catching up
    }
    m_isSearching = false;
    emit isSearchingChanged();
}
