#include "TocDock.h"

#include <QTreeWidget>

TocDock::TocDock(QWidget *parent)
    : QDockWidget(tr("Contents"), parent)
{
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    setWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item, int) {
        emit nodeActivated(item->data(0, Qt::UserRole).value<TocNode>());
    });
}

void TocDock::setTableOfContents(const QVector<TocNode> &toc)
{
    m_tree->clear();
    populate(nullptr, toc);
    m_tree->expandAll();
}

void TocDock::clear()
{
    m_tree->clear();
}

void TocDock::populate(QTreeWidgetItem *parentItem, const QVector<TocNode> &nodes)
{
    for (const TocNode &node : nodes) {
        auto *item = parentItem ? new QTreeWidgetItem(parentItem) : new QTreeWidgetItem(m_tree);
        item->setText(0, node.title);
        item->setData(0, Qt::UserRole, QVariant::fromValue(node));
        populate(item, node.children);
    }
}
