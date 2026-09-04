#pragma once

#include <QObject>
#include <QString>

#include <optional>

class IDocument;
class QTimer;

// Owns PdfView's reading-position persistence and cross-device sync-prompt
// data -- the debounced save timer, ReadingProgressStore/ProgressSyncLog/
// GoogleDriveSync calls, and restoring {page, zoom} on open, all of which
// used to live directly on PdfView. Deliberately does *not* own the live
// current-page/zoom state itself (that stays on PdfView, read constantly by
// its navigation/zoom/scroll logic) -- just persists, restores, and syncs
// whatever PdfView tells it.
//
// HighlightSync::pull() is a separate, highlight-domain sync mechanism that
// used to be triggered from inside the same function as this position-sync
// check purely because they historically lived together in one PdfView
// method; it's called directly from PdfView's constructor now instead of
// through this controller.
class ReadingProgressController : public QObject
{
    Q_OBJECT

public:
    explicit ReadingProgressController(QObject *parent = nullptr);

    struct Restored
    {
        int page;
        qreal zoom;
    };

    // Computes bookHash from filePath's content hash, restores any saved
    // {page, zoom} from ReadingProgressStore (clamped to
    // [0, document->pageCount()-1] / [minZoom, maxZoom]), and kicks off the
    // cross-device remote-position check (local-folder ProgressSyncLog,
    // and Google Drive if signed in) -- remoteProgressAvailable() fires
    // (possibly asynchronously, for the Google Drive leg) if a newer remote
    // position turns up. Returns nullopt if there was nothing saved locally
    // (or bookHash/document is unavailable).
    std::optional<Restored> restore(const QString &filePath, IDocument *document, qreal minZoom, qreal maxZoom);

    QString bookHash() const { return m_bookHash; }

    // (Re)starts the 1500ms debounce.
    void scheduleSave(int currentPage, qreal zoom);
    // Saves immediately if a save is pending; no-op otherwise.
    void flush();

signals:
    void remoteProgressAvailable(int position, qreal zoom, QString deviceName);

private:
    void saveNow();

    QString m_bookHash;
    QString m_title; // cached from IDocument::title() at restore() time, for the sync log's benefit
    int m_pendingPage = 0;
    qreal m_pendingZoom = 1.0;
    QTimer *m_saveTimer;
};
