#pragma once

#include <QString>
#include <QVector>

// Persisted font-family/line-spacing/margin preferences shared by every
// reflowable-text reader (EPUB/MOBI/Markdown/TXT -- not PDF/CBZ, which are
// rasterized pages with no font to customize). Global, like the app's Dark
// Mode setting (see MainWindow's View menu / each view's setDarkMode()),
// not per-document: a reading preference, not a per-book one. Changed via
// TypographyPopup (the "Aa" toolbar button each of those four views adds);
// a newly-opened document picks up whatever's currently set, but changing
// it doesn't retroactively push-update any other already-open tab -- the
// same tradeoff zoom (a genuinely per-book setting) already makes for
// different reasons.
namespace TextReaderTypography {

// Empty means "don't override -- inherit whatever the content/Qt's own
// default specifies", the same convention QFontComboBox's own blank
// selection uses.
QString fontFamily();
void setFontFamily(const QString &family);

// One of lineSpacingPercentSteps()'s values; 100 (CSS's "normal", i.e. no
// override) if never set.
int lineSpacingPercent();
void setLineSpacingPercent(int percent);
QVector<int> lineSpacingPercentSteps(); // offered by TypographyPopup's combo box

// One of marginPxSteps()'s values; 4 (QTextDocument::setDocumentMargin()'s
// own built-in default -- today's existing look, before this setting
// existed) if never set.
int marginPx();
void setMarginPx(int px);
QVector<int> marginPxSteps(); // offered by TypographyPopup's combo box

// CSS for whatever of the above isn't at its "no override" default,
// targeting body,p,div,span -- the same selector scope EpubView/MobiView's
// existing dark-mode color override already uses (see
// EpubView::chapterHtmlFragment()), so a custom font-family/line-height
// actually wins the cascade against the book's own CSS the same way dark
// mode's color override already has to. Empty if both are at default.
// Callers append this to whatever dark-mode/plugin CSS they already build
// (EPUB/MOBI's HTML <style> injection) or hand it to
// QTextDocument::setDefaultStyleSheet() (Markdown) directly -- see each
// view's own applyTypography()-equivalent for which. Margin isn't part of
// this: it's applied via QTextDocument::setDocumentMargin() instead (see
// marginPx()), uniformly across all four views regardless of how each
// builds its content, sidestepping CSS margin/unit differences between
// HTML, Markdown-converted richtext, and plain text entirely.
QString bodyCss();

} // namespace TextReaderTypography
