#include "ReadingProgressController.h"

#include "app/DeviceIdentity.h"
#include "app/FileIdentity.h"
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
#include "app/GoogleDriveSync.h"
#endif
#include "app/ProgressSyncLog.h"
#include "app/ReadingProgressStore.h"
#include "core/Document.h"

#include <QDateTime>
#include <QTimer>

#include <algorithm>

ReadingProgressController::ReadingProgressController(QObject *parent)
    : QObject(parent)
{
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    connect(m_saveTimer, &QTimer::timeout, this, &ReadingProgressController::saveNow);
}

std::optional<ReadingProgressController::Restored> ReadingProgressController::restore(const QString &filePath,
                                                                                        IDocument *document,
                                                                                        qreal minZoom, qreal maxZoom)
{
    m_bookHash = FileIdentity::contentHash(filePath);
    if (m_bookHash.isEmpty() || !document) {
        return std::nullopt;
    }
    m_title = document->title();

    std::optional<Restored> restored;
    if (const auto local = ReadingProgressStore::get(m_bookHash)) {
        restored = Restored{std::clamp(local->position, 0, std::max(0, document->pageCount() - 1)),
                             std::clamp(local->zoom, minZoom, maxZoom)};
    }

    std::optional<ProgressSyncLog::RemoteEntry> localFolderRemote =
        ProgressSyncLog::latestFromOtherDevices(m_bookHash, DeviceIdentity::id());
    if (localFolderRemote) {
        emit remoteProgressAvailable(localFolderRemote->position, localFolderRemote->zoom,
                                      localFolderRemote->deviceName);
    }

#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
    const QString bookHash = m_bookHash;
    const QDateTime localFolderTimestamp = localFolderRemote ? localFolderRemote->timestamp : QDateTime();
    GoogleDriveSync::latestFromOtherDevices(
        bookHash, DeviceIdentity::id(),
        [this, bookHash, localFolderTimestamp](std::optional<ProgressSyncLog::RemoteEntry> googleRemote) {
            if (!googleRemote || bookHash != m_bookHash) {
                return; // no result, or this controller has since moved on to a different book
            }
            if (localFolderTimestamp.isValid() && googleRemote->timestamp <= localFolderTimestamp) {
                return; // the local-folder sync already offered something at least as new
            }
            emit remoteProgressAvailable(googleRemote->position, googleRemote->zoom, googleRemote->deviceName);
        });
#endif

    return restored;
}

void ReadingProgressController::scheduleSave(int currentPage, qreal zoom)
{
    m_pendingPage = currentPage;
    m_pendingZoom = zoom;
    m_saveTimer->start(1500);
}

void ReadingProgressController::flush()
{
    if (!m_saveTimer->isActive()) {
        return;
    }
    m_saveTimer->stop();
    saveNow();
}

void ReadingProgressController::saveNow()
{
    if (m_bookHash.isEmpty()) {
        return;
    }
    ReadingProgressStore::set(m_bookHash, m_pendingPage, m_pendingZoom);
    ProgressSyncLog::appendEntry(m_bookHash, m_title, m_pendingPage, m_pendingZoom);
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
    GoogleDriveSync::appendEntry(m_bookHash, m_title, m_pendingPage, m_pendingZoom);
#endif
}
