#include "BookInfoDock.h"

#include <QFrame>
#include <QLabel>
#include <QPixmap>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace {
constexpr int kCoverWidth = 140;
}

BookInfoDock::BookInfoDock(QWidget *parent)
    : QDockWidget(tr("Book Info"), parent)
{
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    m_coverLabel = new QLabel(container);
    m_coverLabel->setAlignment(Qt::AlignHCenter);
    m_coverLabel->hide();

    m_titleLabel = new QLabel(container);
    m_titleLabel->setWordWrap(true);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 1.0);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->hide();

    m_authorsLabel = new QLabel(container);
    m_authorsLabel->setWordWrap(true);
    m_authorsLabel->hide();

    m_publisherLabel = new QLabel(container);
    m_publisherLabel->setWordWrap(true);
    m_publisherLabel->hide();

    m_publishDateLabel = new QLabel(container);
    m_publishDateLabel->setWordWrap(true);
    m_publishDateLabel->hide();

    m_descriptionLabel = new QLabel(container);
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->hide();

    m_statusLabel = new QLabel(container);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    layout->addWidget(m_coverLabel);
    layout->addWidget(m_titleLabel);
    layout->addWidget(m_authorsLabel);
    layout->addWidget(m_publisherLabel);
    layout->addWidget(m_publishDateLabel);
    layout->addWidget(m_descriptionLabel);
    layout->addWidget(m_statusLabel);
    layout->addStretch(1);

    // A description (local or from Open Library's excerpts) can run to a
    // full paragraph or more, with no bound on its length -- scrollable
    // rather than letting the panel grow past the window or clip it.
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidget(container);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    setWidget(scrollArea);
    clear();
}

void BookInfoDock::showStatus(const QString &text)
{
    m_coverLabel->hide();
    m_titleLabel->hide();
    m_authorsLabel->hide();
    m_publisherLabel->hide();
    m_publishDateLabel->hide();
    m_descriptionLabel->hide();
    m_statusLabel->setText(text);
    m_statusLabel->show();
}

void BookInfoDock::setLoading()
{
    showStatus(tr("Looking up book info…"));
    show();
    raise(); // brings this tab to the front of its tabified group
}

void BookInfoDock::setUnavailable(const QString &reason)
{
    showStatus(reason);
    show();
    raise();
}

void BookInfoDock::clear()
{
    m_coverLabel->hide();
    m_titleLabel->hide();
    m_authorsLabel->hide();
    m_publisherLabel->hide();
    m_publishDateLabel->hide();
    m_descriptionLabel->hide();
    m_statusLabel->hide();

    // Nothing to show for the current tab (no local metadata, no ISBN, or
    // no document open) -- stay out of the tab strip entirely instead of
    // sitting there as a permanent, usually-empty tab; see setLoading()/
    // setMetadata()/setUnavailable() for where it reappears.
    hide();
}

void BookInfoDock::setMetadata(const BookMetadata &metadata)
{
    m_statusLabel->hide();
    show();
    raise();

    if (metadata.cover.isNull()) {
        m_coverLabel->hide();
    } else {
        const QPixmap scaled = QPixmap::fromImage(metadata.cover)
                                    .scaledToWidth(kCoverWidth, Qt::SmoothTransformation);
        m_coverLabel->setPixmap(scaled);
        m_coverLabel->show();
    }

    if (metadata.title.isEmpty()) {
        m_titleLabel->hide();
    } else {
        m_titleLabel->setText(metadata.title);
        m_titleLabel->show();
    }

    if (metadata.authors.isEmpty()) {
        m_authorsLabel->hide();
    } else {
        m_authorsLabel->setText(metadata.authors.join(QStringLiteral(", ")));
        m_authorsLabel->show();
    }

    if (metadata.publisher.isEmpty()) {
        m_publisherLabel->hide();
    } else {
        m_publisherLabel->setText(metadata.publisher);
        m_publisherLabel->show();
    }

    if (metadata.publishDate.isEmpty()) {
        m_publishDateLabel->hide();
    } else {
        m_publishDateLabel->setText(metadata.publishDate);
        m_publishDateLabel->show();
    }

    if (metadata.description.isEmpty()) {
        m_descriptionLabel->hide();
    } else {
        m_descriptionLabel->setText(metadata.description);
        m_descriptionLabel->show();
    }
}
