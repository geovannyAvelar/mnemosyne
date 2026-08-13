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

    // Shows search hits (or an explicit "no results" placeholder if empty).
    void setResults(const QVector<SearchResult> &results);

    // Toggles the busy spinner and disables the query field/button while a
    // background search is in flight, so overlapping searches can't be
    // fired from the same dock.
    void setSearching(bool searching);

    // Resets to the blank pre-search state, e.g. when no document is open.
    void clear();

    void focusSearchField();

signals:
    void searchRequested(const QString &query);
    void resultActivated(int targetIndex);

private:
    QLineEdit *m_queryEdit;
    QPushButton *m_searchButton;
    QProgressBar *m_spinner;
    QListWidget *m_resultsList;
};
