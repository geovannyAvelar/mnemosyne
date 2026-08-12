#pragma once

#include <QToolBar>

// A QToolBar that drags the window when clicked on its empty background,
// standing in for the window-move behavior a native title bar would
// otherwise provide (there isn't one — see MacWindowChrome).
//
// It also permanently suppresses Qt's toolbar-overflow ("qt_toolbar_ext_button")
// mechanism: on this app's dock/toolbar combination, that overflow decision
// has proven unreliable — it can fire (hiding the toolbar's own action
// buttons behind an inaccessible extension button) even when there is
// plainly enough room, and can get stuck that way. The bar's actual content
// is small enough to always fit any reasonable window, so it's simplest to
// never let Qt hide it.
class TopBar : public QToolBar
{
    Q_OBJECT

public:
    explicit TopBar(const QString &title, QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void childEvent(QChildEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
};
