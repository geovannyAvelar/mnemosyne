#include "SearchDock.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

SearchDock::SearchDock(QWidget *parent)
    : QDockWidget(tr("Search"), parent)
{
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *searchRow = new QWidget(container);
    auto *searchRowLayout = new QHBoxLayout(searchRow);
    searchRowLayout->setContentsMargins(0, 0, 0, 0);

    m_queryEdit = new QLineEdit(searchRow);
    m_queryEdit->setPlaceholderText(tr("Search this document..."));
    m_searchButton = new QPushButton(tr("Search"), searchRow);

    auto trigger = [this] {
        const QString query = m_queryEdit->text().trimmed();
        if (!query.isEmpty()) {
            emit searchRequested(query);
        }
    };
    connect(m_queryEdit, &QLineEdit::returnPressed, this, trigger);
    connect(m_searchButton, &QPushButton::clicked, this, trigger);

    searchRowLayout->addWidget(m_queryEdit, 1);
    searchRowLayout->addWidget(m_searchButton);

    // Range (0, 0) puts a QProgressBar into Qt's built-in indeterminate/busy
    // mode — a continuously scrolling bar rather than a fraction-complete
    // one — which is the standard Qt Widgets stand-in for a spinner.
    m_spinner = new QProgressBar(container);
    m_spinner->setRange(0, 0);
    m_spinner->setTextVisible(false);
    m_spinner->hide();

    m_resultsList = new QListWidget(container);
    connect(m_resultsList, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        const QVariant data = item->data(Qt::UserRole);
        if (data.isValid()) {
            emit resultActivated(data.toInt());
        }
    });

    layout->addWidget(searchRow);
    layout->addWidget(m_spinner);
    layout->addWidget(m_resultsList, 1);

    setWidget(container);
}

void SearchDock::setResults(const QVector<SearchResult> &results)
{
    m_resultsList->clear();

    if (results.isEmpty()) {
        auto *placeholder = new QListWidgetItem(tr("No results found."), m_resultsList);
        placeholder->setFlags(Qt::NoItemFlags);
        return;
    }

    for (const SearchResult &result : results) {
        auto *item = new QListWidgetItem(tr("%1\n%2").arg(result.label, result.snippet), m_resultsList);
        item->setData(Qt::UserRole, result.targetIndex);
    }

    // MainWindow jumps to the first hit as soon as results land; reflect
    // that here so the list shows which one the reader landed on.
    m_resultsList->setCurrentRow(0);
}

void SearchDock::setSearching(bool searching)
{
    m_spinner->setVisible(searching);
    m_queryEdit->setEnabled(!searching);
    m_searchButton->setEnabled(!searching);
}

void SearchDock::clear()
{
    m_resultsList->clear();
    m_queryEdit->clear();
    setSearching(false);
}

void SearchDock::focusSearchField()
{
    m_queryEdit->setFocus();
    m_queryEdit->selectAll();
}
