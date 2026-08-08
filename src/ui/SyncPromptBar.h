#pragma once

#include <QWidget>

class QLabel;

// A thin, dismissible banner (not a modal dialog — several tabs could each
// have a pending prompt at once) offering to jump to a reading position
// synced from another device.
class SyncPromptBar : public QWidget
{
    Q_OBJECT

public:
    explicit SyncPromptBar(QWidget *parent = nullptr);

    void showPrompt(const QString &message);

signals:
    void jumpRequested();

private:
    QLabel *m_label = nullptr;
};
