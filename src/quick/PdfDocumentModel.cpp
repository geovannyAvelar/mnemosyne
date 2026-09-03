#include "PdfDocumentModel.h"

#include "ContentUriCache.h"
#include "PdfPageImageProvider.h"

#include "app/DeviceIdentity.h"
#include "app/FileIdentity.h"
#include "app/ReadingProgressStore.h"
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
#include "app/GoogleDriveSync.h"
#endif

#include <QMutexLocker>

#include <algorithm>

namespace {
constexpr qreal kMinZoom = 0.25; // same bounds as desktop PdfView, just continuous rather than stepped
constexpr qreal kMaxZoom = 4.0;
} // namespace

PdfDocumentModel::PdfDocumentModel(PdfPageImageProvider *imageProvider, QObject *parent)
    : QObject(parent)
    , m_imageProvider(imageProvider)
{
    m_progressSaveTimer.setSingleShot(true);
    connect(&m_progressSaveTimer, &QTimer::timeout, this, &PdfDocumentModel::saveProgressNow);
}

PdfDocumentModel::~PdfDocumentModel()
{
    close();
}

void PdfDocumentModel::setErrorMessage(const QString &message)
{
    if (m_errorMessage == message) {
        return;
    }
    m_errorMessage = message;
    emit errorMessageChanged();
}

bool PdfDocumentModel::open(const QString &filePathOrUri, const QString &format)
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
    std::unique_ptr<IDocument> document = openDocument(localPath, &openError);
    if (!document) {
        setErrorMessage(openError.isEmpty() ? QStringLiteral("Could not open PDF") : openError);
        return false;
    }

    m_document = std::move(document);
    m_bookHash = FileIdentity::contentHash(localPath);
    m_imageProvider->setDocument(m_document.get());

    restoreProgress();

    emit documentChanged();
    emit currentPageChanged();
    emit zoomChanged();
    return true;
}

void PdfDocumentModel::close()
{
    if (!m_document) {
        return;
    }
    if (m_progressSaveTimer.isActive()) {
        m_progressSaveTimer.stop();
        saveProgressNow();
    }
    m_imageProvider->setDocument(nullptr); // blocks until in-flight renders against this document finish
    m_document.reset();
    m_bookHash.clear();
    m_currentPage = 0;
    m_zoom = 1.0;
    emit documentChanged();
}

void PdfDocumentModel::setCurrentPage(int page)
{
    if (!m_document) {
        return;
    }
    const int clamped = std::clamp(page, 0, std::max(0, m_document->pageCount() - 1));
    if (clamped == m_currentPage) {
        return;
    }
    m_currentPage = clamped;
    emit currentPageChanged();
    m_progressSaveTimer.start(1500);
}

void PdfDocumentModel::setZoom(qreal zoom)
{
    const qreal clamped = std::clamp(zoom, kMinZoom, kMaxZoom);
    if (qFuzzyCompare(clamped, m_zoom)) {
        return;
    }
    m_zoom = clamped;
    emit zoomChanged();
    m_progressSaveTimer.start(1500);
}

QSizeF PdfDocumentModel::pageSizePoints(int index) const
{
    if (!m_document) {
        return {};
    }
    // PdfPageImageProvider's worker threads render from this same document
    // (see PdfPageImageProvider::documentMutex()) — this call happens on
    // the UI thread, so it needs the same lock to avoid racing an in-flight
    // render inside Poppler, which isn't safe under concurrent access.
    QMutexLocker locker(&m_imageProvider->documentMutex());
    std::unique_ptr<IPage> page = m_document->page(index);
    return page ? page->sizePoints() : QSizeF();
}

QVector<TextWord> PdfDocumentModel::wordsForPage(int index) const
{
    if (!m_document) {
        return {};
    }
    // See pageSizePoints() above — same race, this time triggered by a
    // long-press starting a text selection while a page is still rendering.
    QMutexLocker locker(&m_imageProvider->documentMutex());
    std::unique_ptr<IPage> page = m_document->page(index);
    return page ? page->words() : QVector<TextWord>();
}

void PdfDocumentModel::restoreProgress()
{
    m_currentPage = 0;
    m_zoom = 1.0;
    if (m_bookHash.isEmpty()) {
        return;
    }
    if (const auto progress = ReadingProgressStore::get(m_bookHash)) {
        m_currentPage = std::clamp(progress->position, 0, std::max(0, pageCount() - 1));
        m_zoom = std::clamp(progress->zoom, kMinZoom, kMaxZoom);
    }

#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
    // Async — matches desktop PdfView::restoreProgressAndCheckSync(), minus
    // the local-folder SyncFolder/ProgressSyncLog leg, which has no Android
    // analog (see the mobile-port plan's Context section).
    const QString bookHash = m_bookHash;
    GoogleDriveSync::latestFromOtherDevices(
        bookHash, DeviceIdentity::id(), [this, bookHash](std::optional<ProgressSyncLog::RemoteEntry> remote) {
            if (!remote || bookHash != m_bookHash) {
                return; // no result, or this model has since moved on to a different book
            }
            if (remote->position == m_currentPage) {
                return;
            }
            emit remoteProgressAvailable(remote->position, remote->zoom, remote->deviceName);
        });
#endif
}

void PdfDocumentModel::saveProgressNow()
{
    if (m_bookHash.isEmpty()) {
        return;
    }
    ReadingProgressStore::set(m_bookHash, m_currentPage, m_zoom);
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
    GoogleDriveSync::appendEntry(m_bookHash, title(), m_currentPage, m_zoom);
#endif
}
