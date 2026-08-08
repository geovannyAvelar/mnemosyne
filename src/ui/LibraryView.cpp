#include "LibraryView.h"

#include "app/RecentFiles.h"

#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

LibraryView::LibraryView(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);

    auto *title = new QLabel(tr("<h2>Library</h2>"), this);

    auto *openButton = new QPushButton(tr("Open Document..."), this);
    connect(openButton, &QPushButton::clicked, this, &LibraryView::openRequested);

    m_list = new QListWidget(this);
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        emit fileActivated(item->data(Qt::UserRole).toString());
    });

    layout->addWidget(title);
    layout->addWidget(openButton, 0, Qt::AlignLeft);
    layout->addWidget(new QLabel(tr("Recent Documents"), this));
    layout->addWidget(m_list, 1);

    refresh();
}

void LibraryView::refresh()
{
    m_list->clear();

    const QVector<RecentFiles::Entry> entries = RecentFiles::list();
    if (entries.isEmpty()) {
        auto *placeholder = new QListWidgetItem(tr("No recent documents yet."), m_list);
        placeholder->setFlags(Qt::NoItemFlags);
        return;
    }

    for (const RecentFiles::Entry &entry : entries) {
        const QString text = tr("%1\n%2 • %3")
                                  .arg(entry.title.isEmpty() ? entry.filePath : entry.title,
                                       entry.format.toUpper(),
                                       QLocale().toString(entry.lastOpened, QLocale::ShortFormat));
        auto *item = new QListWidgetItem(text, m_list);
        item->setData(Qt::UserRole, entry.filePath);
        item->setToolTip(entry.filePath);
    }
}
