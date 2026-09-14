#pragma once

#include "core/ReaderView.h"

#include <QDockWidget>

class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;

class SearchDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit SearchDock(QWidget *parent = nullptr);

    // Shows search hits (or an explicit "no results" placeholder if empty)
    // all at once -- still used by the formats whose searchFile() blocks
    // until the whole document has been scanned (see MainWindow.cpp's
    // searchRequested handler). EPUB search instead streams in one hit at
    // a time via clearResults()/appendResult()/finishResults() below.
    void setResults(const QVector<SearchResult> &results);

    // Clears the list for a search that's about to start streaming in
    // results one at a time, leaving the "no results" placeholder out
    // until finishResults() confirms none arrived.
    void clearResults();

    // Adds one hit to the list as it's found, without touching whatever's
    // already there.
    void appendResult(const SearchResult &result);

    // Call once a streamed search has stopped (finished or cancelled) --
    // shows the "no results" placeholder if appendResult() was never
    // called, and selects the first hit if it was.
    void finishResults();

    // Toggles the busy spinner and disables the query field/button while a
    // background search is in flight, so overlapping searches can't be
    // fired from the same dock. cancelable shows a Cancel button wired to
    // cancelRequested() -- only EPUB search (see the class comment on
    // clearResults()) can actually act on it, so every other format leaves
    // this false and the button stays hidden.
    void setSearching(bool searching, bool cancelable = false);

    // Resets to the blank pre-search state, e.g. when no document is open.
    void clear();

    void focusSearchField();

signals:
    void searchRequested(const QString &query);
    void resultActivated(int targetIndex);
    // EPUB search only (see setSearching()'s doc comment) -- the other
    // formats' blocking searchFile() calls can't be interrupted mid-scan.
    void cancelRequested();

private:
    QLineEdit *m_queryEdit;
    QPushButton *m_searchButton;
    QPushButton *m_cancelButton;
    QProgressBar *m_spinner;
    QListWidget *m_resultsList;
};
