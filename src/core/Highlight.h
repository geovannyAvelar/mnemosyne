#pragma once

#include <QColor>
#include <QDateTime>
#include <QRectF>
#include <QString>

// Translucent yellow — the highlight color used before per-highlight color
// existed, kept as the default for plain "Highlight" and as the fallback
// when reading a highlight persisted before this field existed.
inline const QColor kDefaultHighlightColor(255, 235, 59, 140);

struct Highlight
{
    int targetIndex = -1; // PDF page index, EPUB/MOBI spine (or "part") index, or 0 for Markdown (single document)
    QRectF pageRect; // PDF only, in page points; null for EPUB/Markdown/MOBI
    QString text; // the highlighted text (also used to relocate it in EPUB/Markdown/MOBI's reflowed text)
    QDateTime createdAt;
    QString note; // optional user-written comment attached to the highlight; empty if none
    QColor color = kDefaultHighlightColor; // marker color, chosen from a palette when adding a note

    // Fields below are appended (not interleaved above) so existing positional
    // aggregate-init call sites, e.g. Highlight{page, rect, text, createdAt},
    // keep compiling unchanged.
    QString id; // stable UUID, used to identify this highlight across devices for sync; see HighlightSync
    QDateTime updatedAt; // bumped on every local edit; falls back to createdAt when absent (pre-sync data)
    quint64 lamportClock = 0; // bumped (LamportClock::tick()) alongside updatedAt; 0 = no Lamport info yet (pre-migration data) -- see SyncOrdering.h
};
