#pragma once

#include <QMutex>
#include <QQuickAsyncImageProvider>

class IDocument;

// Renders PDF pages on a worker thread (via QThreadPool, one page-render at
// a time) so pinch-zoom and page navigation never block the UI thread the
// way desktop PdfView's synchronous IPage::renderToImage() call does.
//
// image id format: "<pageIndex>-<scale>", e.g. "3-1.5" for page index 3 at
// 1.5x. Registered once in main_android.cpp under the "pdfpage" scheme, so
// QML requests pages via "image://pdfpage/<pageIndex>-<scale>".
class PdfPageImageProvider : public QQuickAsyncImageProvider
{
public:
    // Called by PdfDocumentModel with the currently-open document (or
    // nullptr when closing/replacing one). Blocks until any render task
    // already in flight against the *previous* document finishes, so it's
    // safe for the caller to delete that document immediately after this
    // call returns — an in-flight task never touches a pointer this class
    // has already been told to stop using, because every task re-reads the
    // document pointer from here (under the same lock) at render time
    // rather than capturing it up front.
    void setDocument(IDocument *document);

    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

    // The lock worker-thread renders take around every access to the
    // shared document (see PdfPageImageResponse::run()). PdfDocumentModel
    // holds the same IDocument this provider renders from and calls
    // IDocument::page() straight from the UI thread for sizing/word-lookup
    // (pageSizePoints(), wordsForPage()) — Poppler's Document/Page types
    // aren't safe under concurrent access, so those calls must take this
    // same lock or they can race an in-flight render and crash.
    QMutex &documentMutex() { return m_mutex; }

private:
    friend class PdfPageImageResponse;
    QMutex m_mutex;
    IDocument *m_document = nullptr; // non-owning
};
