#pragma once

#include <QWidget>

class QComboBox;
class QListWidget;
class QListWidgetItem;
class ThumbnailProvider;

class LibraryView : public QWidget
{
    Q_OBJECT

public:
    explicit LibraryView(QWidget *parent = nullptr);

    void refresh();

signals:
    void fileActivated(const QString &filePath);
    void openRequested();

private:
    // Rebuilds the shelf sidebar (m_shelfList) and tag filter (m_tagFilter)
    // from CollectionStore/TagStore -- called by refresh() so a shelf
    // created/renamed/deleted via the book context menu shows up without
    // needing a separate explicit refresh call at every call site.
    void refreshFilters();
    // The book grid item's context menu -- shared by the two chunks that
    // used to be inline (Remove from Recent alone) now that it also offers
    // Add to Shelf / Edit Tags.
    void showItemContextMenu(const QPoint &pos);

    QListWidget *m_list;
    // Sidebar of shelves: "All Books" (row 0, no filter) followed by every
    // CollectionStore collection. Selecting one filters m_list to just that
    // shelf's books -- combined (AND) with m_tagFilter's own selection, if
    // any.
    QListWidget *m_shelfList;
    QComboBox *m_tagFilter;
    QString m_selectedCollection; // empty = "All Books" (no shelf filter)
    QString m_selectedTag; // empty = "All Tags" (no tag filter)
    ThumbnailProvider *m_thumbnailProvider;
};
