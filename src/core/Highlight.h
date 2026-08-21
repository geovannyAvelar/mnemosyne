#pragma once

#include <QDateTime>
#include <QRectF>
#include <QString>

struct Highlight
{
    int targetIndex = -1; // PDF page index, EPUB/MOBI spine (or "part") index, or 0 for Markdown (single document)
    QRectF pageRect; // PDF only, in page points; null for EPUB/Markdown/MOBI
    QString text; // the highlighted text (also used to relocate it in EPUB/Markdown/MOBI's reflowed text)
    QDateTime createdAt;
};
