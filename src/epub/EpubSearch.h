#pragma once

#include "core/ReaderView.h"

#include <QString>

#include <atomic>
#include <functional>
#include <memory>

// Shared by every caller of searchEpubFile() below to request an in-flight
// search stop early -- a std::shared_ptr so the GUI thread (MainWindow's
// Cancel button, SearchResultsModel::cancel()) can flip it after the
// search's own local copy on the worker thread would otherwise be the only
// owner left.
using EpubSearchCancelToken = std::shared_ptr<std::atomic_bool>;

// Always returns a fresh, not-yet-cancelled token.
EpubSearchCancelToken makeSearchCancelToken();

// Case-insensitive full-text search across every chapter of the EPUB at
// filePath, invoking onResult once per matching chapter (first match per
// chapter only -- same coarse, chapter-level scope desktop's search has
// always had) as soon as it's found, rather than collecting everything
// into a QVector first. Lets a caller running this on a worker thread
// (see MainWindow.cpp's m_epubSearchWatcher, quick/SearchResultsModel.cpp)
// show hits incrementally instead of waiting for the whole book to be
// scanned.
//
// Checks cancelToken between chapters and returns early once it's set, so
// a scan of a huge book can be aborted without waiting for it to finish;
// pass makeSearchCancelToken()'s result and store it to cancel later, or a
// null token to never cancel.
//
// Opens its own EpubDocument rather than taking one, since libzip reads
// aren't safe to share across threads -- callers typically run this on a
// worker thread while a live view keeps its own EpubDocument on the main
// thread. onResult is called synchronously on whatever thread this
// function itself runs on; a caller updating UI from it must hop back to
// the GUI thread itself (e.g. QMetaObject::invokeMethod(guiObject, ...,
// Qt::QueuedConnection), the pattern EpubView.cpp's own background-thread
// work already uses).
void searchEpubFile(const QString &filePath, const QString &query,
                     const std::function<void(const SearchResult &)> &onResult,
                     const EpubSearchCancelToken &cancelToken);
