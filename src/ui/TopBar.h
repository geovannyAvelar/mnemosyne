#pragma once

#include <QToolBar>

// A QToolBar that drags the window when clicked on its empty background,
// standing in for the window-move behavior a native title bar would
// otherwise provide (there isn't one — see MacWindowChrome).
class TopBar : public QToolBar
{
    Q_OBJECT

public:
    explicit TopBar(const QString &title, QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
};
