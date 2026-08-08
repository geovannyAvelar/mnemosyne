#pragma once

#include <QWidget>

class QListWidget;

class LibraryView : public QWidget
{
    Q_OBJECT

public:
    explicit LibraryView(QWidget *parent = nullptr);

    void refresh();

signals:
    void fileActivated(const QString &filePath);
    void openRequested();

private:
    QListWidget *m_list;
};
