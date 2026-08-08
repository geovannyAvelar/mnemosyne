#include "SyncPromptBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

SyncPromptBar::SyncPromptBar(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet(QStringLiteral(
        "SyncPromptBar { background-color: #D97757; }"
        "SyncPromptBar QPushButton { background: rgba(255,255,255,40); color: white; border: none; border-radius: 6px; padding: 5px 12px; }"
        "SyncPromptBar QPushButton:hover { background: rgba(255,255,255,70); }"));
    setAutoFillBackground(true);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);

    m_label = new QLabel(this);
    m_label->setStyleSheet(QStringLiteral("color: white; font-weight: 500;"));

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
