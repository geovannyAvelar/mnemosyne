#include "LibraryView.h"

#include "ThumbnailProvider.h"
#include "app/RecentFiles.h"

#include <QAction>
#include <QFileInfo>
#include <QFont>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

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
    connect(m_list, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QListWidgetItem *item = m_list->itemAt(pos);
        // The empty-state placeholder item has no UserRole path (see
        // refresh()), so it's excluded here too rather than needing its own
        // separate flags check.
        const QString filePath = item ? item->data(Qt::UserRole).toString() : QString();
        if (filePath.isEmpty()) {
            return;
        }

        QMenu menu(m_list);
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
    });

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
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);

        const QPixmap cached = m_thumbnailProvider->request(entry.filePath, entry.format);
        item->setIcon(QIcon(cached.isNull() ? placeholderThumbnail(entry.format) : cached));
    }
}
