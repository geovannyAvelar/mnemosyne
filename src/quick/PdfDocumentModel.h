#pragma once

#include "core/Document.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>

class PdfPageImageProvider;

// QML-facing counterpart to what IReaderView/PdfView give MainWindow on
// desktop: opens a PDF via the same unmodified openDocument() (see
// core/Document.h), tracks current page/zoom, and persists reading
// position via app/ReadingProgressStore.h keyed by content hash — all
// exactly as desktop does. The difference is delivery: page *rendering*
// goes through PdfPageImageProvider on a worker thread instead of a
// synchronous QImage returned straight to a QPainter, and zoom is
// continuous (pinch) rather than desktop's discrete +/- steps.
class PdfDocumentModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY documentChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY documentChanged)
    Q_PROPERTY(QString title READ title NOTIFY documentChanged)
    Q_PROPERTY(QString bookHash READ bookHash NOTIFY documentChanged)
    Q_PROPERTY(int currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    explicit PdfDocumentModel(PdfPageImageProvider *imageProvider, QObject *parent = nullptr);
    ~PdfDocumentModel() override;

    bool isOpen() const { return m_document != nullptr; }
    int pageCount() const { return m_document ? m_document->pageCount() : 0; }
    QString title() const { return m_document ? m_document->title() : QString(); }
    // Content hash used to key BookmarkStore/ReadingProgressStore entries
    // for the currently-open document — see BookmarksModel::bookHash.
    QString bookHash() const { return m_bookHash; }
    int currentPage() const { return m_currentPage; }
    void setCurrentPage(int page);
    qreal zoom() const { return m_zoom; }
    void setZoom(qreal zoom);
    QString errorMessage() const { return m_errorMessage; }

    // filePathOrUri: a real path (desktop) or a content:// URI (Android,
    // from AndroidStorageAccess) — see ContentUriCache for how the latter
    // gets resolved to a real local path before anything else touches it.
    Q_INVOKABLE bool open(const QString &filePathOrUri, const QString &format);
    Q_INVOKABLE void close();

    // Page size in points at scale 1.0, for QML to size each SwipeView
    // page item's aspect ratio before its image has finished rendering.
    Q_INVOKABLE QSizeF pageSizePoints(int index) const;

    // Word boxes for page index, in reading order and page-space points —
    // for PdfSelectionController's word-snapping selection. Not QML-facing
    // (QVector<TextWord> isn't a QML value type), called directly from C++.
    QVector<TextWord> wordsForPage(int index) const;

signals:
    void documentChanged();
    void currentPageChanged();
    void zoomChanged();
    void errorMessageChanged();
    // A more recent position for this document was found on another
    // device via GoogleDriveSync — see restoreProgress(). Only emitted
    // when it differs from the position just restored locally.
    void remoteProgressAvailable(int position, qreal zoom, const QString &deviceName);

private:
    void setErrorMessage(const QString &message);
    void restoreProgress();
    void saveProgressNow();

    PdfPageImageProvider *m_imageProvider; // non-owning
    std::unique_ptr<IDocument> m_document;
    QString m_bookHash;
    int m_currentPage = 0;
    qreal m_zoom = 1.0;
    QString m_errorMessage;
    QTimer m_progressSaveTimer;
};
