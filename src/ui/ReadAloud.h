#pragma once

#include <QString>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocumentFragment>

// Implemented by reader views whose text can be spoken. ReadAloudController
// asks for the text, speaks it, then asks the view to move on.
class IReadAloudView
{
public:
    virtual ~IReadAloudView() = default;

    // Plain text from the current reading position to the end of the
    // current unit (page for PDF; visible position to chapter/document
    // end for flowing text).
    virtual QString readAloudText() const = 0;

    // Moves to the next unit synchronously and returns true, or returns
    // false when there is no next unit (or it can't be reached
    // synchronously -- EPUB/MOBI chapters load asynchronously, so they stop
    // at the end of the chapter instead).
    virtual bool readAloudAdvance() = 0;
};

namespace ReadAloud {

// Text from the top of the visible area to the end of a QTextBrowser's
// document: what the reader is looking at, and everything after it.
inline QString textFromViewport(const QTextBrowser *browser)
{
    QTextCursor cursor = browser->cursorForPosition(QPoint(1, 1));
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    return cursor.selection().toPlainText();
}

} // namespace ReadAloud
