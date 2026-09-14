#pragma once

#include "core/ReaderView.h"

#include <QString>
#include <QVector>

// Case-insensitive full-text search across every chapter of the EPUB at
// filePath, one SearchResult per matching chapter (first match only — same
// coarse-grained, chapter-level scope as desktop's EpubView::searchFile,
// which this function's logic was extracted from so both the desktop
// (ui/EpubView.cpp) and QML (quick/SearchResultsModel.cpp) front ends stay
// in sync). Opens its own EpubDocument rather than taking one, since
// libzip reads aren't safe to share across threads and callers typically
// run this on a worker thread while a live view keeps its own EpubDocument
// on the main thread.
QVector<SearchResult> searchEpubFile(const QString &filePath, const QString &query);
