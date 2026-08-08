#include "SyncPromptBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

SyncPromptBar::SyncPromptBar(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet(QStringLiteral("SyncPromptBar { background-color: #2d5c8a; }"));
    setAutoFillBackground(true);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 6, 10, 6);

    m_label = new QLabel(this);
    m_label->setStyleSheet(QStringLiteral("color: white;"));

    auto *jumpButton = new QPushButton(tr("Jump"), this);
    auto *dismissButton = new QPushButton(tr("Dismiss"), this);
    connect(jumpButton, &QPushButton::clicked, this, [this] {
        hide();
        emit jumpRequested();
    });
    connect(dismissButton, &QPushButton::clicked, this, &SyncPromptBar::hide);

    layout->addWidget(m_label, 1);
    layout->addWidget(jumpButton);
    layout->addWidget(dismissButton);

    hide();
}

void SyncPromptBar::showPrompt(const QString &message)
{
    m_label->setText(message);
    show();
}
