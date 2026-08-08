#pragma once

#include "core/ReaderView.h"

#include <QDockWidget>

class QLineEdit;
class QListWidget;

class SearchDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit SearchDock(QWidget *parent = nullptr);

    // Shows search hits (or an explicit "no results" placeholder if empty).
    void setResults(const QVector<SearchResult> &results);

    // Resets to the blank pre-search state, e.g. when no document is open.
    void clear();

    void focusSearchField();

signals:
    void searchRequested(const QString &query);
    void resultActivated(int targetIndex);

private:
    QLineEdit *m_queryEdit;
    QListWidget *m_resultsList;
};
