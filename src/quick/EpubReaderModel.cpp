#include "EpubReaderModel.h"

#include "ContentUriCache.h"

#include "app/DeviceIdentity.h"
#include "app/FileIdentity.h"
#include "app/ReadingProgressStore.h"
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
#include "app/GoogleDriveSync.h"
#endif

#include <algorithm>

EpubReaderModel::EpubReaderModel(QObject *parent)
    : QObject(parent)
{
    m_progressSaveTimer.setSingleShot(true);
    connect(&m_progressSaveTimer, &QTimer::timeout, this, &EpubReaderModel::saveProgressNow);
}

EpubReaderModel::~EpubReaderModel()
{
    close();
}

void EpubReaderModel::setErrorMessage(const QString &message)
{
    if (m_errorMessage == message) {
        return;
    }
    m_errorMessage = message;
    emit errorMessageChanged();
}

bool EpubReaderModel::open(const QString &filePathOrUri, const QString &format)
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
    std::unique_ptr<EpubDocument> document = EpubDocument::load(localPath, &openError);
    if (!document) {
        setErrorMessage(openError.isEmpty() ? QStringLiteral("Could not open EPUB") : openError);
        return false;
    }

    m_document = std::move(document);
    m_bookHash = FileIdentity::contentHash(localPath);

    restoreProgress();

    emit documentChanged();
    emit currentSpineIndexChanged();
    return true;
}

void EpubReaderModel::close()
{
    if (!m_document) {
        return;
    }
    if (m_progressSaveTimer.isActive()) {
        m_progressSaveTimer.stop();
        saveProgressNow();
    }
    m_document.reset();
    m_bookHash.clear();
    m_currentSpineIndex = 0;
    emit documentChanged();
}

void EpubReaderModel::setCurrentSpineIndex(int index)
{
    if (!m_document) {
        return;
    }
    const int clamped = std::clamp(index, 0, std::max(0, m_document->spineCount() - 1));
    if (clamped == m_currentSpineIndex) {
        return;
    }
    m_currentSpineIndex = clamped;
    emit currentSpineIndexChanged();
    m_progressSaveTimer.start(1500);
}

QString EpubReaderModel::currentChapterHtml() const
{
    return m_document ? m_document->chapterHtml(m_currentSpineIndex) : QString();
}

void EpubReaderModel::nextChapter()
{
    setCurrentSpineIndex(m_currentSpineIndex + 1);
}

void EpubReaderModel::previousChapter()
{
    setCurrentSpineIndex(m_currentSpineIndex - 1);
}

void EpubReaderModel::restoreProgress()
{
    m_currentSpineIndex = 0;
    if (m_bookHash.isEmpty()) {
        return;
    }
    if (const auto progress = ReadingProgressStore::get(m_bookHash)) {
        m_currentSpineIndex = std::clamp(progress->position, 0, std::max(0, spineCount() - 1));
    }

#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
    const QString bookHash = m_bookHash;
    GoogleDriveSync::latestFromOtherDevices(
        bookHash, DeviceIdentity::id(), [this, bookHash](std::optional<ProgressSyncLog::RemoteEntry> remote) {
            if (!remote || bookHash != m_bookHash) {
                return;
            }
            if (remote->position == m_currentSpineIndex) {
                return;
            }
            emit remoteProgressAvailable(remote->position, remote->deviceName);
        });
#endif
}

void EpubReaderModel::saveProgressNow()
{
    if (m_bookHash.isEmpty()) {
        return;
    }
    ReadingProgressStore::set(m_bookHash, m_currentSpineIndex, 1.0);
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
    GoogleDriveSync::appendEntry(m_bookHash, title(), m_currentSpineIndex, 1.0);
#endif
}
