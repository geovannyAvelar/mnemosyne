#include "NoteDialog.h"

#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <functional>

namespace {

// Matches kDefaultHighlightColor's alpha so every preset reads consistently
// against both light and dark page backgrounds.
constexpr int kAlpha = 140;

const QVector<QColor> &presetColors()
{
    static const QVector<QColor> kPresets = {
        QColor(255, 235, 59, kAlpha), // yellow
        QColor(76, 175, 80, kAlpha), // green
        QColor(33, 150, 243, kAlpha), // blue
        QColor(233, 30, 99, kAlpha), // pink
        QColor(255, 152, 0, kAlpha), // orange
    };
    return kPresets;
}

QString swatchStyle(const QColor &color, bool selected)
{
    QColor opaque = color;
    opaque.setAlpha(255);
    const QString border = selected ? QStringLiteral("2px solid palette(highlight)") : QStringLiteral("1px solid palette(mid)");
    return QStringLiteral("background-color: %1; border: %2;").arg(opaque.name(), border);
}

} // namespace

namespace NoteDialog {

std::optional<Result> show(QWidget *parent, const QString &initialNote, const QColor &initialColor)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Note"));

    auto *layout = new QVBoxLayout(&dialog);

    auto *noteEdit = new QPlainTextEdit(&dialog);
    noteEdit->setPlainText(initialNote);
    noteEdit->setMinimumSize(320, 120);
    layout->addWidget(noteEdit);

    layout->addWidget(new QLabel(QObject::tr("Color:"), &dialog));

    auto *colorRow = new QHBoxLayout;
    layout->addLayout(colorRow);

    QColor selectedColor = initialColor;
    QVector<QPushButton *> swatchButtons;

    auto *preview = new QFrame(&dialog);
    preview->setFixedSize(28, 28);
    preview->setFrameShape(QFrame::Box);

    // Declared before selectColor since its lambda calls it; assigned below
    // once the swatch buttons it needs to update actually exist.
    std::function<void()> refreshSwatches;

    auto selectColor = [&](const QColor &color) {
        selectedColor = color;
        refreshSwatches();
    };

    for (const QColor &preset : presetColors()) {
        auto *swatch = new QPushButton(&dialog);
        swatch->setFixedSize(28, 28);
        colorRow->addWidget(swatch);
        swatchButtons.append(swatch);
        QObject::connect(swatch, &QPushButton::clicked, &dialog, [selectColor, preset] { selectColor(preset); });
    }

    refreshSwatches = [&] {
        const QVector<QColor> &presets = presetColors();
        for (int i = 0; i < swatchButtons.size(); ++i) {
            swatchButtons[i]->setStyleSheet(swatchStyle(presets[i], presets[i] == selectedColor));
        }
        QColor opaquePreview = selectedColor;
        opaquePreview.setAlpha(255);
        preview->setStyleSheet(QStringLiteral("background-color: %1; border: 2px solid palette(highlight);").arg(opaquePreview.name()));
    };
    refreshSwatches();

    colorRow->addWidget(preview);

    auto *customButton = new QPushButton(QObject::tr("More Colors..."), &dialog);
    colorRow->addWidget(customButton);
    QObject::connect(customButton, &QPushButton::clicked, &dialog, [&] {
        const QColor picked = QColorDialog::getColor(selectedColor, &dialog, QObject::tr("Choose a Color"), QColorDialog::ShowAlphaChannel);
        if (picked.isValid()) {
            selectColor(picked);
        }
    });

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }

    return Result{noteEdit->toPlainText(), selectedColor};
}

} // namespace NoteDialog
