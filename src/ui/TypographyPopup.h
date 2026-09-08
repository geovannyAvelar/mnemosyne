#pragma once

#include <QPoint>
#include <QString>

#include <functional>

class QWidget;

// Lightweight, non-modal "Aa" popup for the font-family/line-spacing/margin
// controls each of EpubView/MarkdownView/MobiView/TxtView's toolbar exposes
// -- mirrors NotePopup's shape (Qt::Popup, dismisses on outside click) but
// for live-editing TextReaderTypography's settings rather than displaying a
// note. Applies changes immediately as each control changes (no OK/Apply
// button), matching how zoom's +/- buttons already behave.
namespace TypographyPopup {

// currentFamily/currentLineSpacingPercent/currentMarginPx seed the controls'
// initial state (typically straight from TextReaderTypography's own
// getters); onChanged fires on every control change with the full updated
// triple (not just what changed), so callers can always just re-apply and
// re-persist all three without tracking which one moved.
void show(QWidget *parent, const QPoint &globalPos, const QString &currentFamily, int currentLineSpacingPercent,
          int currentMarginPx,
          std::function<void(const QString &family, int lineSpacingPercent, int marginPx)> onChanged);

} // namespace TypographyPopup
