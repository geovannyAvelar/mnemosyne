#include "TextReaderModel.h"

#include "ContentUriCache.h"

#include "app/DeviceIdentity.h"
#include "app/FileIdentity.h"
#include "app/ReadingProgressStore.h"
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
#include "app/GoogleDriveSync.h"
#endif

#include <algorithm>

TextReaderModel::TextReaderModel(QObject *parent)
    : QObject(parent)
{
    m_progressSaveTimer.setSingleShot(true);
    connect(&m_progressSaveTimer, &QTimer::timeout, this, &TextReaderModel::saveProgressNow);
}

TextReaderModel::~TextReaderModel()
{
    close();
}

void TextReaderModel::setErrorMessage(const QString &message)
{
    if (m_errorMessage == message) {
        return;
    }
    m_errorMessage = message;
    emit errorMessageChanged();
}

bool TextReaderModel::open(const QString &filePathOrUri, const QString &format)
{
    close();
    setErrorMessage(QString());

    QString resolveError;
    const QString localPath = ContentUriCache::resolveToLocalFile(filePathOrUri, format, &resolveError);
    if (localPath.isEmpty()) {
        setErrorMessage(resolveError.isEmpty() ? QStringLiteral("Could not access the picked document")
                                                : resolveError);
        return false;
    }

    QString openError;
    std::optional<TextParts> document = loadTextParts(localPath, &openError);
    if (!document) {
        setErrorMessage(openError.isEmpty() ? QStringLiteral("Could not open document") : openError);
        return false;
    }

    m_document = std::move(*document);
    m_bookHash = FileIdentity::contentHash(localPath);
    m_filePath = localPath;

    restoreProgress();

    emit documentChanged();
    emit currentSpineIndexChanged();
    return true;
}

void TextReaderModel::close()
{
    if (!isOpen()) {
        return;
    }
    if (m_progressSaveTimer.isActive()) {
        m_progressSaveTimer.stop();
        saveProgressNow();
    }
    m_document = TextParts();
    m_bookHash.clear();
    m_filePath.clear();
    m_currentSpineIndex = 0;
    m_zoom = 0;
    emit documentChanged();
}

void TextReaderModel::setCurrentSpineIndex(int index)
{
    if (!isOpen()) {
        return;
    }
    const int clamped = std::clamp(index, 0, spineCount() - 1);
    if (clamped == m_currentSpineIndex) {
        return;
    }
    m_currentSpineIndex = clamped;
    emit currentSpineIndexChanged();
    m_progressSaveTimer.start(1500);
}

QString TextReaderModel::currentChapterHtml() const
{
    return isOpen() ? textPartHtml(m_document.parts.at(m_currentSpineIndex), m_document.isMarkdown) : QString();
}

void TextReaderModel::nextChapter()
{
    setCurrentSpineIndex(m_currentSpineIndex + 1);
}

void TextReaderModel::previousChapter()
{
    setCurrentSpineIndex(m_currentSpineIndex - 1);
}

void TextReaderModel::restoreProgress()
{
    m_currentSpineIndex = 0;
    m_zoom = 0;
    if (m_bookHash.isEmpty()) {
        return;
    }
    if (const auto progress = ReadingProgressStore::get(m_bookHash)) {
        m_currentSpineIndex = partIndexForPosition(m_document.parts, progress->position);
        m_zoom = progress->zoom;
    }

#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
    const QString bookHash = m_bookHash;
    GoogleDriveSync::latestFromOtherDevices(
        bookHash, DeviceIdentity::id(), [this, bookHash](std::optional<ProgressSyncLog::RemoteEntry> remote) {
            if (!remote || bookHash != m_bookHash) {
                return;
            }
            const int remoteIndex = partIndexForPosition(m_document.parts, remote->position);
            if (remoteIndex == m_currentSpineIndex) {
                return;
            }
            emit remoteProgressAvailable(remoteIndex, remote->deviceName);
        });
#endif
}

void TextReaderModel::saveProgressNow()
{
    if (m_bookHash.isEmpty()) {
        return;
    }
    const int position = m_document.parts.at(m_currentSpineIndex).position;
    ReadingProgressStore::set(m_bookHash, position, m_zoom);
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
    GoogleDriveSync::appendEntry(m_bookHash, title(), position, m_zoom);
#endif
}
