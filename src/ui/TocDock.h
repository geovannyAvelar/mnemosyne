#pragma once

#include "core/Document.h"

#include <QDockWidget>

class QTreeWidget;
class QTreeWidgetItem;

class TocDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit TocDock(QWidget *parent = nullptr);

    void setTableOfContents(const QVector<TocNode> &toc);
    void clear();

signals:
    void nodeActivated(const TocNode &node);

private:
    void populate(QTreeWidgetItem *parentItem, const QVector<TocNode> &nodes);

    QTreeWidget *m_tree;
};
