#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>

class IReadAloudView;
class QTextToSpeech;
class QWidget;

// Speaks the active reader view with the platform speech engine
// (speech-dispatcher on Linux, SAPI on Windows, AVSpeechSynthesizer on
// macOS) via QtTextToSpeech.
class ReadAloudController : public QObject
{
    Q_OBJECT

public:
    explicit ReadAloudController(QObject *parent = nullptr);

    // False when Qt found no speech engine on this system.
    bool isAvailable() const;
    bool isActive() const { return m_active; }

    // view must also be a QWidget (all reader views are).
    void start(IReadAloudView *view);
    void stop();

signals:
    void activeChanged(bool active);

private:
    void speakNextChunk();
    void onStateChanged();
    void finish();

    QTextToSpeech *m_speech = nullptr;
    IReadAloudView *m_view = nullptr;
    QPointer<QWidget> m_viewWidget; // guards m_view against the tab being closed
    QStringList m_chunks;
    bool m_active = false;
    bool m_waitingForChunk = false;
};
