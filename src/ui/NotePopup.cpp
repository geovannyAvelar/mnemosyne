#include "NotePopup.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

#include <algorithm>

namespace NotePopup {

void show(QWidget *parent, const QPoint &globalPos, const QString &noteText, std::function<void()> onEdit,
          std::function<void()> onRemove)
{
    // Qt::Popup closes itself on any click outside it (or Escape, or
    // losing focus) with no extra event-filter plumbing needed -- the same
    // mechanism QMenu relies on, just without menu semantics/keyboard
    // navigation. WA_DeleteOnClose since nothing else owns this afterward.
    auto *popup = new QFrame(parent, Qt::Popup);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setFrameShape(QFrame::StyledPanel);
    popup->setStyleSheet(QStringLiteral("QFrame { background: palette(base); border: 1px solid palette(mid); }"));

    auto *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(6);

    auto *label = new QLabel(noteText, popup);
    label->setWordWrap(true);
    label->setMaximumWidth(280);
    layout->addWidget(label);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(buttonRow);

    auto *editButton = new QPushButton(QObject::tr("Edit"), popup);
    editButton->setFlat(true);
    QObject::connect(editButton, &QPushButton::clicked, popup, [popup, onEdit] {
        popup->close();
        onEdit();
    });
    buttonRow->addWidget(editButton);

    auto *removeButton = new QPushButton(QObject::tr("Remove"), popup);
    removeButton->setFlat(true);
    QObject::connect(removeButton, &QPushButton::clicked, popup, [popup, onRemove] {
        popup->close();
        onRemove();
    });
    buttonRow->addWidget(removeButton);
    buttonRow->addStretch();

    popup->adjustSize();

    // Clamp inside whichever screen globalPos is on, so a note clicked near
    // the window's right/bottom edge doesn't render partly off-screen.
    QPoint origin = globalPos;
    if (QScreen *screen = QApplication::screenAt(globalPos)) {
        const QRect avail = screen->availableGeometry();
        origin.setX(std::min(origin.x(), avail.right() - popup->width()));
        origin.setY(std::min(origin.y(), avail.bottom() - popup->height()));
    }
    popup->move(origin);
    popup->show();
}

} // namespace NotePopup
