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
    // Extra top margin beyond the other sides' 32px: on macOS the window's
    // close/minimize/fullscreen buttons and the search action live in the
    // TopBar toolbar directly above this view (see MainWindow::setupSidebarToggle),
    // so the title needs enough clearance to never visually sit in that row.
    layout->setContentsMargins(32, 64, 32, 32);
    layout->setSpacing(14);

    auto *title = new QLabel(tr("Library"), this);
    title->setObjectName(QStringLiteral("libraryTitle"));

    auto *openButton = new QPushButton(tr("Open Document..."), this);
    openButton->setObjectName(QStringLiteral("primaryButton"));
    openButton->setCursor(Qt::PointingHandCursor);
    connect(openButton, &QPushButton::clicked, this, &LibraryView::openRequested);

    auto *recentLabel = new QLabel(tr("RECENT DOCUMENTS"), this);
    recentLabel->setObjectName(QStringLiteral("sectionLabel"));

    m_list = new QListWidget(this);
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        emit fileActivated(item->data(Qt::UserRole).toString());
    });

    layout->addWidget(title);
    layout->addSpacing(4);
    layout->addWidget(openButton, 0, Qt::AlignLeft);
    layout->addSpacing(12);
    layout->addWidget(recentLabel);
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
