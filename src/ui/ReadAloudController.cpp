#include "ReadAloudController.h"

#include "ui/ReadAloud.h"

#include <QRegularExpression>
#include <QTextToSpeech>
#include <QWidget>

namespace {

// Some engines choke on very long utterances and none report progress per
// sentence usefully, so text is fed in paragraph-sized chunks.
constexpr int kMaxChunkChars = 1500;

QStringList splitIntoChunks(const QString &text)
{
    QStringList chunks;
    QString current;
    QString normalized = text;
    normalized.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
    const QStringList paragraphs = normalized.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &paragraph : paragraphs) {
        const QString trimmed = paragraph.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (!current.isEmpty() && current.size() + trimmed.size() + 1 > kMaxChunkChars) {
            chunks << current;
            current.clear();
        }
        // A single paragraph longer than the limit: split at sentence ends.
        QString rest = trimmed;
        while (rest.size() > kMaxChunkChars) {
            int cut = rest.lastIndexOf(QRegularExpression(QStringLiteral("[.!?]\\s")), kMaxChunkChars);
            cut = cut > 0 ? cut + 1 : kMaxChunkChars;
            if (!current.isEmpty()) {
                chunks << current;
                current.clear();
            }
            chunks << rest.left(cut).trimmed();
            rest = rest.mid(cut).trimmed();
        }
        current += (current.isEmpty() ? QString() : QStringLiteral("\n")) + rest;
    }
    if (!current.isEmpty()) {
        chunks << current;
    }
    return chunks;
}

} // namespace

ReadAloudController::ReadAloudController(QObject *parent)
    : QObject(parent)
{
    if (QTextToSpeech::availableEngines().isEmpty()) {
        return;
    }
    m_speech = new QTextToSpeech(this);
    connect(m_speech, &QTextToSpeech::stateChanged, this, &ReadAloudController::onStateChanged);
}

bool ReadAloudController::isAvailable() const
{
    return m_speech != nullptr && m_speech->state() != QTextToSpeech::Error;
}

void ReadAloudController::start(IReadAloudView *view)
{
    if (!isAvailable() || !view) {
        return;
    }
    stop();
    m_view = view;
    m_viewWidget = dynamic_cast<QWidget *>(view);
    m_chunks = splitIntoChunks(view->readAloudText());
    m_active = true;
    emit activeChanged(true);
    speakNextChunk();
}

void ReadAloudController::stop()
{
    if (!m_active) {
        return;
    }
    m_chunks.clear();
    m_waitingForChunk = false;
    m_active = false; // before stop(): its stateChanged must not advance
    m_speech->stop();
    m_view = nullptr;
    emit activeChanged(false);
}

void ReadAloudController::speakNextChunk()
{
    if (!m_viewWidget) {
        finish();
        return;
    }
    if (m_chunks.isEmpty()) {
        // Unit done: move on (PDF page) and read it, or end.
        if (!m_view->readAloudAdvance()) {
            finish();
            return;
        }
        m_chunks = splitIntoChunks(m_view->readAloudText());
        if (m_chunks.isEmpty()) {
            // Blank page (e.g. a scanned image): keep going rather than stop.
            speakNextChunk();
            return;
        }
    }
    m_waitingForChunk = true;
    m_speech->say(m_chunks.takeFirst());
}

void ReadAloudController::onStateChanged()
{
    if (!m_active) {
        return;
    }
    if (m_speech->state() == QTextToSpeech::Ready && m_waitingForChunk) {
        m_waitingForChunk = false;
        speakNextChunk();
    } else if (m_speech->state() == QTextToSpeech::Error) {
        finish();
    }
}

void ReadAloudController::finish()
{
    m_chunks.clear();
    m_waitingForChunk = false;
    m_view = nullptr;
    if (m_active) {
        m_active = false;
        emit activeChanged(false);
    }
}
