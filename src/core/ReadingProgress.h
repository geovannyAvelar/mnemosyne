#pragma once

#include <QDateTime>
#include <QString>

// A snapshot of "where you are" in a book: page index (PDF), spine/"part"
// index (EPUB/MOBI), heading index (Markdown, -1 meaning "before any
// heading"), or always 0 (HTML), plus a format-specific zoom value
// normalized to a single qreal (PdfView's render scale, EpubView's/
// MarkdownView's/MobiView's font-zoom steps, HtmlView's zoomFactor — each
// view converts to/from its own representation).
struct ReadingProgress
{
    int position = 0;
    qreal zoom = 1.0;
    QDateTime updatedAt;
};
