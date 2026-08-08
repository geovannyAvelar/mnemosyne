#pragma once

#include "core/Bookmark.h"

#include <QDockWidget>

class QListWidget;

class BookmarksDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit BookmarksDock(QWidget *parent = nullptr);

    void setBookmarks(const QVector<Bookmark> &bookmarks);
    void clear();

signals:
    void bookmarkActivated(int targetIndex);
    void addBookmarkRequested();
    void removeBookmarkRequested(int index); // index into the list passed to setBookmarks

private:
    QListWidget *m_list;
};
