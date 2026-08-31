#include "TopBar.h"

#include <QChildEvent>
#include <QDebug>
#include <QMouseEvent>
#include <QWindow>

namespace {
const QString kExtensionButtonName = QStringLiteral("qt_toolbar_ext_button");
}

TopBar::TopBar(const QString &title, QWidget *parent)
    : QToolBar(title, parent)
{
}

void TopBar::mousePressEvent(QMouseEvent *event)
{
    qDebug() << "DIAG TopBar::mousePressEvent pos=" << event->pos() << "childAt=" << childAt(event->pos());
    if (event->button() == Qt::LeftButton && !childAt(event->pos())) {
        if (QWindow *handle = window()->windowHandle()) {
            handle->startSystemMove();
            return;
        }
    }
    QToolBar::mousePressEvent(event);
}

void TopBar::childEvent(QChildEvent *event)
{
    QToolBar::childEvent(event);
    if (event->type() == QEvent::ChildAdded) {
        if (auto *child = qobject_cast<QWidget *>(event->child())) {
            child->installEventFilter(this);
        }
    }
}

bool TopBar::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Show) {
        if (auto *widget = qobject_cast<QWidget *>(watched)) {
            if (widget->objectName() == kExtensionButtonName) {
                widget->hide();
                return true;
            }
        }
    }
    return QToolBar::eventFilter(watched, event);
}
