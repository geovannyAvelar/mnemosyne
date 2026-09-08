#include "LibraryView.h"

#include "ThumbnailProvider.h"
#include "app/CollectionStore.h"
#include "app/RecentFiles.h"
#include "app/TagStore.h"

#include <QAction>
#include <QComboBox>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int kAllBooksRow = 0; // m_shelfList's fixed first row -- no shelf filter
constexpr int kAllTagsIndex = 0; // m_tagFilter's fixed first entry -- no tag filter

// Shown until (or unless) ThumbnailProvider produces a real preview: a
// plain dog-eared page bearing the format label, drawn instead of shipped
// as an asset so it stays crisp at the thumbnail's device pixel ratio and
// needs no theme-specific variant (transparent background, muted-alpha
// strokes read fine on both light and dark).
QPixmap placeholderThumbnail(const QString &format)
{
    const QSize size = ThumbnailProvider::thumbnailSize();
    const qreal dpr = 2.0;

    QPixmap pixmap(size * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    const QColor stroke(0x8A, 0x87, 0x80, 150);
    const QColor fill(0x8A, 0x87, 0x80, 26);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const qreal margin = 10.0;
    const qreal foldSize = 18.0;
    const QRectF rect(margin, margin, size.width() - margin * 2, size.height() - margin * 2);

    QPainterPath page;
    page.moveTo(rect.left(), rect.top());
    page.lineTo(rect.right() - foldSize, rect.top());
    page.lineTo(rect.right(), rect.top() + foldSize);
    page.lineTo(rect.right(), rect.bottom());
    page.lineTo(rect.left(), rect.bottom());
    page.closeSubpath();

    painter.setPen(QPen(stroke, 1.4));
    painter.setBrush(fill);
    painter.drawPath(page);

    QPainterPath fold;
    fold.moveTo(rect.right() - foldSize, rect.top());
    fold.lineTo(rect.right() - foldSize, rect.top() + foldSize);
    fold.lineTo(rect.right(), rect.top() + foldSize);
    painter.drawPath(fold);

    QFont font = painter.font();
    font.setPointSizeF(9.0);
    font.setWeight(QFont::DemiBold);
    painter.setFont(font);
    painter.setPen(stroke);
    painter.drawText(rect.adjusted(4, 0, -4, -12), Qt::AlignBottom | Qt::AlignHCenter, format.toUpper());

    return pixmap;
}

} // namespace

LibraryView::LibraryView(QWidget *parent)
    : QWidget(parent)
    , m_thumbnailProvider(new ThumbnailProvider(this))
{
    auto *outerLayout = new QVBoxLayout(this);
    // Extra top margin beyond the other sides' 32px: on macOS the window's
    // close/minimize/fullscreen buttons and the search action live in the
    // TopBar toolbar directly above this view (see MainWindow::setupSidebarToggle),
    // so the title needs enough clearance to never visually sit in that row.
    outerLayout->setContentsMargins(32, 64, 32, 32);
    outerLayout->setSpacing(14);

    auto *title = new QLabel(tr("Library"), this);
    title->setObjectName(QStringLiteral("libraryTitle"));

    auto *openButton = new QPushButton(tr("Open Document..."), this);
    openButton->setObjectName(QStringLiteral("primaryButton"));
    openButton->setCursor(Qt::PointingHandCursor);
    connect(openButton, &QPushButton::clicked, this, &LibraryView::openRequested);

    outerLayout->addWidget(title);
    outerLayout->addSpacing(4);
    outerLayout->addWidget(openButton, 0, Qt::AlignLeft);
    outerLayout->addSpacing(12);

    // Shelves sidebar (left) + tag filter/grid (right) -- a shelf and a tag
    // filter combine (AND) rather than being mutually exclusive, so e.g.
    // "Sci-Fi" shelf + "favorite" tag narrows to books that are both.
    auto *bodyLayout = new QHBoxLayout();
    bodyLayout->setSpacing(20);

    auto *shelfColumn = new QVBoxLayout();
    auto *shelfLabel = new QLabel(tr("SHELVES"), this);
    shelfLabel->setObjectName(QStringLiteral("sectionLabel"));
    m_shelfList = new QListWidget(this);
    m_shelfList->setFixedWidth(160);
    connect(m_shelfList, &QListWidget::currentRowChanged, this, [this](int row) {
        m_selectedCollection = row > kAllBooksRow ? m_shelfList->item(row)->text() : QString();
        refresh();
    });
    auto *newShelfButton = new QPushButton(tr("+ New Shelf"), this);
    connect(newShelfButton, &QPushButton::clicked, this, [this] {
        const QString name =
            QInputDialog::getText(this, tr("New Shelf"), tr("Shelf name:")).trimmed();
        if (name.isEmpty()) {
            return;
        }
        CollectionStore::createCollection(name);
        refresh();
    });
    shelfColumn->addWidget(shelfLabel);
    shelfColumn->addWidget(m_shelfList, 1);
    shelfColumn->addWidget(newShelfButton);

    auto *gridColumn = new QVBoxLayout();
    gridColumn->setSpacing(14);

    auto *filterRow = new QHBoxLayout();
    auto *recentLabel = new QLabel(tr("RECENT DOCUMENTS"), this);
    recentLabel->setObjectName(QStringLiteral("sectionLabel"));
    m_tagFilter = new QComboBox(this);
    connect(m_tagFilter, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_selectedTag = index > kAllTagsIndex ? m_tagFilter->itemText(index) : QString();
        refresh();
    });
    filterRow->addWidget(recentLabel);
    filterRow->addStretch();
    filterRow->addWidget(new QLabel(tr("Tag:"), this));
    filterRow->addWidget(m_tagFilter);

    const QSize thumbSize = ThumbnailProvider::thumbnailSize();

    m_list = new QListWidget(this);
    m_list->setViewMode(QListView::IconMode);
    m_list->setMovement(QListView::Static);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setWrapping(true);
    m_list->setUniformItemSizes(true);
    m_list->setSpacing(12);
    m_list->setIconSize(thumbSize);
    m_list->setGridSize(QSize(thumbSize.width() + 28, thumbSize.height() + 64));
    m_list->setWordWrap(true);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        emit fileActivated(item->data(Qt::UserRole).toString());
    });
    connect(m_list, &QListWidget::customContextMenuRequested, this, &LibraryView::showItemContextMenu);

    connect(m_thumbnailProvider, &ThumbnailProvider::thumbnailReady, this,
            [this](const QString &filePath, const QPixmap &pixmap) {
                for (int i = 0; i < m_list->count(); ++i) {
                    QListWidgetItem *item = m_list->item(i);
                    if (item->data(Qt::UserRole).toString() == filePath) {
                        item->setIcon(QIcon(pixmap));
                        break;
                    }
                }
            });

    gridColumn->addLayout(filterRow);
    gridColumn->addWidget(m_list, 1);

    bodyLayout->addLayout(shelfColumn);
    bodyLayout->addLayout(gridColumn, 1);
    outerLayout->addLayout(bodyLayout, 1);

    refresh();
}

void LibraryView::refreshFilters()
{
    // Rebuild the shelf list, preserving the current selection by name
    // (rather than row index, since a shelf earlier in the alphabetically-
    // unsorted creation-order list could shift if one before it was
    // deleted).
    {
        const QSignalBlocker blocker(m_shelfList);
        m_shelfList->clear();
        m_shelfList->addItem(tr("All Books"));
        for (const QString &name : CollectionStore::allCollections()) {
            m_shelfList->addItem(name);
        }
        int rowToSelect = kAllBooksRow;
        if (!m_selectedCollection.isEmpty()) {
            const QList<QListWidgetItem *> matches = m_shelfList->findItems(m_selectedCollection, Qt::MatchExactly);
            if (!matches.isEmpty()) {
                rowToSelect = m_shelfList->row(matches.first());
            } else {
                m_selectedCollection.clear(); // the selected shelf was deleted elsewhere
            }
        }
        m_shelfList->setCurrentRow(rowToSelect);
    }

    {
        const QSignalBlocker blocker(m_tagFilter);
        m_tagFilter->clear();
        m_tagFilter->addItem(tr("All Tags"));
        m_tagFilter->addItems(TagStore::allTags());
        int indexToSelect = kAllTagsIndex;
        if (!m_selectedTag.isEmpty()) {
            const int found = m_tagFilter->findText(m_selectedTag);
            if (found >= 0) {
                indexToSelect = found;
            } else {
                m_selectedTag.clear(); // no book carries this tag anymore
            }
        }
        m_tagFilter->setCurrentIndex(indexToSelect);
    }
}

void LibraryView::refresh()
{
    refreshFilters();
    m_list->clear();

    QVector<RecentFiles::Entry> entries = RecentFiles::list();
    if (!m_selectedCollection.isEmpty()) {
        const QStringList members = CollectionStore::booksInCollection(m_selectedCollection);
        entries.erase(std::remove_if(entries.begin(), entries.end(),
                                      [&](const RecentFiles::Entry &e) {
                                          return e.contentHash.isEmpty() || !members.contains(e.contentHash);
                                      }),
                      entries.end());
    }
    if (!m_selectedTag.isEmpty()) {
        const QStringList tagged = TagStore::booksWithTag(m_selectedTag);
        entries.erase(std::remove_if(entries.begin(), entries.end(),
                                      [&](const RecentFiles::Entry &e) {
                                          return e.contentHash.isEmpty() || !tagged.contains(e.contentHash);
                                      }),
                      entries.end());
    }

    if (entries.isEmpty()) {
        auto *placeholder = new QListWidgetItem(
            m_selectedCollection.isEmpty() && m_selectedTag.isEmpty() ? tr("No recent documents yet.")
                                                                       : tr("No books match this filter."),
            m_list);
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
        item->setData(Qt::UserRole + 1, entry.contentHash);
        item->setToolTip(entry.filePath);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);

        const QPixmap cached = m_thumbnailProvider->request(entry.filePath, entry.format);
        item->setIcon(QIcon(cached.isNull() ? placeholderThumbnail(entry.format) : cached));
    }
}

void LibraryView::showItemContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_list->itemAt(pos);
    // The empty-state placeholder item has no UserRole path (see refresh()),
    // so it's excluded here too rather than needing its own separate flags
    // check.
    const QString filePath = item ? item->data(Qt::UserRole).toString() : QString();
    if (filePath.isEmpty()) {
        return;
    }
    const QString bookHash = item->data(Qt::UserRole + 1).toString();

    QMenu menu(m_list);

    // A pre-hash Recent entry (see RecentFiles::Entry::contentHash's doc
    // comment) has nothing stable to key a shelf/tag assignment on --
    // re-opening the book once it's re-recorded fixes this, so rather than
    // introduce a path-keyed fallback storage scheme just for a
    // disappearing edge case, these two actions are simply hidden for it.
    if (!bookHash.isEmpty()) {
        QMenu *shelfMenu = menu.addMenu(tr("Add to Shelf"));
        const QStringList currentShelves = CollectionStore::collectionsForBook(bookHash);
        for (const QString &name : CollectionStore::allCollections()) {
            QAction *action = shelfMenu->addAction(name);
            action->setCheckable(true);
            action->setChecked(currentShelves.contains(name));
            connect(action, &QAction::toggled, this, [this, bookHash, name](bool checked) {
                if (checked) {
                    CollectionStore::addBookToCollection(bookHash, name);
                } else {
                    CollectionStore::removeBookFromCollection(bookHash, name);
                }
                refresh();
            });
        }
        if (!CollectionStore::allCollections().isEmpty()) {
            shelfMenu->addSeparator();
        }
        QAction *newShelfAction = shelfMenu->addAction(tr("New Shelf..."));
        connect(newShelfAction, &QAction::triggered, this, [this, bookHash] {
            const QString name =
                QInputDialog::getText(this, tr("New Shelf"), tr("Shelf name:")).trimmed();
            if (name.isEmpty()) {
                return;
            }
            CollectionStore::addBookToCollection(bookHash, name);
            refresh();
        });

        QAction *tagsAction = menu.addAction(tr("Edit Tags..."));
        connect(tagsAction, &QAction::triggered, this, [this, bookHash] {
            const QString current = TagStore::tagsForBook(bookHash).join(QStringLiteral(", "));
            bool ok = false;
            const QString entered = QInputDialog::getText(this, tr("Edit Tags"),
                                                            tr("Comma-separated tags:"), QLineEdit::Normal, current,
                                                            &ok);
            if (!ok) {
                return;
            }
            TagStore::setTagsForBook(bookHash, entered.split(QLatin1Char(','), Qt::SkipEmptyParts));
            refresh();
        });

        menu.addSeparator();
    }

    QAction *removeAction = menu.addAction(tr("Remove from Recent"));
    connect(removeAction, &QAction::triggered, this, [this, filePath] {
        // Explicit default button (No): Enter/Return confirms the safe
        // choice rather than the removal, matching how other "are you
        // sure" prompts default to the non-destructive option.
        const auto choice = QMessageBox::question(
            this, tr("Remove from Recent"),
            tr("Remove \"%1\" from Recent Documents?\n\nThe file itself won't be deleted.")
                .arg(QFileInfo(filePath).fileName()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (choice != QMessageBox::Yes) {
            return;
        }
        RecentFiles::remove(filePath);
        refresh();
    });

    menu.exec(m_list->mapToGlobal(pos));
}
