#pragma once

#include <QDateTime>
#include <QRectF>
#include <QString>

struct Highlight
{
    int targetIndex = -1; // PDF page index or EPUB spine index
    QRectF pageRect; // PDF only, in page points; null for EPUB
    QString text; // the highlighted text (also used to relocate it in EPUB's reflowed HTML)
    QDateTime createdAt;
};
