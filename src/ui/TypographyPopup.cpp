#include "TypographyPopup.h"

#include "ui/TextReaderTypography.h"

#include <QApplication>
#include <QComboBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

namespace TypographyPopup {

void show(QWidget *parent, const QPoint &globalPos, const QString &currentFamily, int currentLineSpacingPercent,
          int currentMarginPx,
          std::function<void(const QString &family, int lineSpacingPercent, int marginPx)> onChanged)
{
    // Qt::Popup closes itself on any click outside it (or Escape, or losing
    // focus) with no extra event-filter plumbing needed -- see NotePopup,
    // which this mirrors.
    auto *popup = new QFrame(parent, Qt::Popup);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setFrameShape(QFrame::StyledPanel);
    popup->setStyleSheet(QStringLiteral("QFrame { background: palette(base); border: 1px solid palette(mid); }"));

    auto *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(6);

    auto *form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(form);

    // QFontComboBox has no "no selection" state of its own, so an explicit
    // Reset button is how a reader gets back to "inherit the content/Qt's
    // own default" (an empty family in TextReaderTypography) rather than
    // being stuck on whatever real font the combo box happens to show.
    auto *fontRow = new QHBoxLayout;
    auto *fontCombo = new QFontComboBox(popup);
    if (!currentFamily.isEmpty()) {
        fontCombo->setCurrentFont(QFont(currentFamily));
    }
    fontRow->addWidget(fontCombo, 1);
    auto *resetFontButton = new QPushButton(QObject::tr("Reset"), popup);
    fontRow->addWidget(resetFontButton);
    form->addRow(QObject::tr("Font:"), fontRow);

    auto *lineSpacingCombo = new QComboBox(popup);
    const QVector<int> spacingSteps = TextReaderTypography::lineSpacingPercentSteps();
    for (int percent : spacingSteps) {
        lineSpacingCombo->addItem(QObject::tr("%1%").arg(percent), percent);
    }
    lineSpacingCombo->setCurrentIndex(std::max(0, int(spacingSteps.indexOf(currentLineSpacingPercent))));
    form->addRow(QObject::tr("Line spacing:"), lineSpacingCombo);

    auto *marginCombo = new QComboBox(popup);
    const QVector<int> marginSteps = TextReaderTypography::marginPxSteps();
    const QStringList marginLabels = {QObject::tr("Narrow"), QObject::tr("Medium"), QObject::tr("Wide")};
    for (int i = 0; i < marginSteps.size(); ++i) {
        marginCombo->addItem(i < marginLabels.size() ? marginLabels[i] : QString::number(marginSteps[i]),
                              marginSteps[i]);
    }
    marginCombo->setCurrentIndex(std::max(0, int(marginSteps.indexOf(currentMarginPx))));
    form->addRow(QObject::tr("Margins:"), marginCombo);

    // Re-reads all three controls' current state rather than tracking which
    // one fired -- see this header's own doc comment for why onChanged's
    // contract is "the full triple, every time".
    auto emitChange = [=] {
        const QString family = fontCombo->currentFont().family();
        const int spacing = lineSpacingCombo->currentData().toInt();
        const int margin = marginCombo->currentData().toInt();
        onChanged(family, spacing, margin);
    };

    QObject::connect(fontCombo, &QFontComboBox::currentFontChanged, popup, [=](const QFont &) { emitChange(); });
    QObject::connect(resetFontButton, &QPushButton::clicked, popup, [=] {
        QSignalBlocker blocker(fontCombo);
        fontCombo->setCurrentFont(QFont());
        onChanged(QString(), lineSpacingCombo->currentData().toInt(), marginCombo->currentData().toInt());
    });
    QObject::connect(lineSpacingCombo, &QComboBox::currentIndexChanged, popup, [=](int) { emitChange(); });
    QObject::connect(marginCombo, &QComboBox::currentIndexChanged, popup, [=](int) { emitChange(); });

    popup->adjustSize();

    // Clamp inside whichever screen globalPos is on, so the popup doesn't
    // render partly off-screen near the window's right/bottom edge.
    QPoint origin = globalPos;
    if (QScreen *screen = QApplication::screenAt(globalPos)) {
        const QRect avail = screen->availableGeometry();
        origin.setX(std::min(origin.x(), avail.right() - popup->width()));
        origin.setY(std::min(origin.y(), avail.bottom() - popup->height()));
    }
    popup->move(origin);
    popup->show();
}

} // namespace TypographyPopup
