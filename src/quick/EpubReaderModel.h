#pragma once

#include "epub/EpubDocument.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>

// QML-facing counterpart to what IReaderView/EpubView give MainWindow on
// desktop, wrapping the same unmodified EpubDocument (see epub/EpubDocument.h)
// and persisting reading position via ReadingProgressStore exactly as
// desktop's EpubView does (position = spine index). Chapter *rendering*
// goes to Qt's WebView module (see qml/EpubReaderScreen.qml) instead of a
// QTextBrowser — WebView.loadHtml() consumes chapterHtml()'s
// already-self-contained HTML (inlined CSS, data: URI images) directly.
//
// Desktop's per-chapter font-zoom (EpubView's m_fontZoomSteps) has no
// analog here: Qt WebView's QML API exposes no zoom control at all (see
// ~/Qt/6.8.3/*/qml/QtWebView/plugins.qmltypes — url/loading/title/
// loadHtml/runJavaScript, nothing else). Zoom is saved as a fixed 1.0 for
// now; real font-size control would need injecting CSS via runJavaScript,
// deferred rather than built speculatively before it's needed.
class EpubReaderModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY documentChanged)
    Q_PROPERTY(int spineCount READ spineCount NOTIFY documentChanged)
    Q_PROPERTY(QString title READ title NOTIFY documentChanged)
    Q_PROPERTY(int currentSpineIndex READ currentSpineIndex WRITE setCurrentSpineIndex NOTIFY currentSpineIndexChanged)
    Q_PROPERTY(QString currentChapterHtml READ currentChapterHtml NOTIFY currentSpineIndexChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    explicit EpubReaderModel(QObject *parent = nullptr);
    ~EpubReaderModel() override;

    bool isOpen() const { return m_document != nullptr; }
    int spineCount() const { return m_document ? m_document->spineCount() : 0; }
    QString title() const { return m_document ? m_document->title() : QString(); }
    int currentSpineIndex() const { return m_currentSpineIndex; }
    void setCurrentSpineIndex(int index);
    QString currentChapterHtml() const;
    QString errorMessage() const { return m_errorMessage; }

    // filePathOrUri: a real path (desktop) or a content:// URI (Android) —
    // see ContentUriCache, used the same way PdfDocumentModel uses it.
    Q_INVOKABLE bool open(const QString &filePathOrUri, const QString &format);
    Q_INVOKABLE void close();
    Q_INVOKABLE void nextChapter();
    Q_INVOKABLE void previousChapter();

signals:
    void documentChanged();
    void currentSpineIndexChanged();
    void errorMessageChanged();

private:
    void setErrorMessage(const QString &message);
    void restoreProgress();
    void saveProgressNow();

    std::unique_ptr<EpubDocument> m_document;
    QString m_bookHash;
    int m_currentSpineIndex = 0;
    QString m_errorMessage;
    QTimer m_progressSaveTimer;
};
