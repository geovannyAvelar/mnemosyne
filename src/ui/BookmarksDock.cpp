#include "BookmarksDock.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

BookmarksDock::BookmarksDock(QWidget *parent)
    : QDockWidget(tr("Bookmarks"), parent)
{
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *toolbar = new QWidget(container);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);

    auto *addButton = new QPushButton(tr("+ Add"), toolbar);
    auto *removeButton = new QPushButton(tr("- Remove"), toolbar);
    connect(addButton, &QPushButton::clicked, this, &BookmarksDock::addBookmarkRequested);
    connect(removeButton, &QPushButton::clicked, this, [this] {
        const int row = m_list->currentRow();
        if (row >= 0) {
            emit removeBookmarkRequested(row);
        }
    });

    toolbarLayout->addWidget(addButton);
    toolbarLayout->addWidget(removeButton);
    toolbarLayout->addStretch();

    m_list = new QListWidget(container);
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        emit bookmarkActivated(item->data(Qt::UserRole).toInt());
    });

    layout->addWidget(toolbar);
    layout->addWidget(m_list, 1);

    setWidget(container);
}

void BookmarksDock::setBookmarks(const QVector<Bookmark> &bookmarks)
{
    m_list->clear();
    for (const Bookmark &bookmark : bookmarks) {
        const QString text = bookmark.label.isEmpty() ? tr("Position %1").arg(bookmark.targetIndex + 1) : bookmark.label;
        auto *item = new QListWidgetItem(text, m_list);
        item->setData(Qt::UserRole, bookmark.targetIndex);
    }
}

void BookmarksDock::clear()
{
    m_list->clear();
}
