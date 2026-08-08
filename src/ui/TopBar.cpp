#include "TopBar.h"

#include <QMouseEvent>
#include <QWindow>

TopBar::TopBar(const QString &title, QWidget *parent)
    : QToolBar(title, parent)
{
}

void TopBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !childAt(event->pos())) {
        if (QWindow *handle = window()->windowHandle()) {
            handle->startSystemMove();
            return;
        }
    }
    QToolBar::mousePressEvent(event);
}
