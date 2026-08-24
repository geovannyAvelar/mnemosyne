#include "LibraryModel.h"

#include "app/RecentFiles.h"

int LibraryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_rows.size();
}

QVariant LibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }

    const Row &row = m_rows.at(index.row());
    switch (role) {
    case FilePathRole:
        return row.filePath;
    case TitleRole:
        return row.title.isEmpty() ? row.filePath : row.title;
    case FormatRole:
        return row.format;
    case LastOpenedRole:
        return row.lastOpened;
    default:
        return {};
    }
}

QHash<int, QByteArray> LibraryModel::roleNames() const
{
    return {
        {FilePathRole, "filePath"},
        {TitleRole, "title"},
        {FormatRole, "format"},
        {LastOpenedRole, "lastOpened"},
    };
}

void LibraryModel::refresh()
{
    beginResetModel();
    m_rows.clear();
    const QVector<RecentFiles::Entry> entries = RecentFiles::list();
    m_rows.reserve(entries.size());
    for (const RecentFiles::Entry &entry : entries) {
        m_rows.append({entry.filePath, entry.title, entry.format, entry.lastOpened});
    }
    endResetModel();
}

void LibraryModel::recordOpened(const QString &filePath, const QString &title, const QString &format)
{
    RecentFiles::recordOpened(filePath, title, format);
    refresh();
}

void LibraryModel::removeEntry(const QString &filePath)
{
    RecentFiles::remove(filePath);
    refresh();
}
