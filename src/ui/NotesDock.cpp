#include "NotesDock.h"

#include <QColor>
#include <QHBoxLayout>
#include <QIcon>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace {

QIcon colorSwatchIcon(const QColor &color)
{
    QPixmap pixmap(14, 14);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor opaque = color;
    opaque.setAlpha(255);
    painter.setBrush(opaque);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(pixmap.rect().adjusted(1, 1, -1, -1));
    return QIcon(pixmap);
}

} // namespace

NotesDock::NotesDock(QWidget *parent)
    : QDockWidget(tr("Notes"), parent)
{
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *toolbar = new QWidget(container);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);

    auto *editButton = new QPushButton(tr("Edit..."), toolbar);
    auto *removeButton = new QPushButton(tr("- Remove"), toolbar);
    connect(editButton, &QPushButton::clicked, this, [this] {
        const int row = m_list->currentRow();
        if (row >= 0) {
            emit editNoteRequested(m_list->item(row)->data(Qt::UserRole).toInt());
        }
    });
    connect(removeButton, &QPushButton::clicked, this, [this] {
        const int row = m_list->currentRow();
        if (row >= 0) {
            emit removeNoteRequested(m_list->item(row)->data(Qt::UserRole).toInt());
        }
    });

    toolbarLayout->addWidget(editButton);
    toolbarLayout->addWidget(removeButton);
    toolbarLayout->addStretch();

    m_list = new QListWidget(container);
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        emit noteActivated(item->data(Qt::UserRole + 1).toInt());
    });

    layout->addWidget(toolbar);
    layout->addWidget(m_list, 1);

    setWidget(container);
}

void NotesDock::setHighlights(const QVector<Highlight> &highlights)
{
    m_list->clear();
    for (int i = 0; i < highlights.size(); ++i) {
        const Highlight &highlight = highlights[i];
        if (highlight.note.isEmpty()) {
            continue;
        }
        auto *item = new QListWidgetItem(highlight.note, m_list);
        item->setData(Qt::UserRole, i); // original index, for HighlightStore calls
        item->setData(Qt::UserRole + 1, highlight.targetIndex); // for jump-to
        item->setIcon(colorSwatchIcon(highlight.color));
        if (!highlight.text.isEmpty()) {
            item->setToolTip(highlight.text); // the passage the note is attached to
        }
    }
}

void NotesDock::clear()
{
    m_list->clear();
}
