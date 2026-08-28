#pragma once

#include "core/ReaderView.h"

#include <QWidget>

class QLabel;
class QTimer;
class QWebEngineView;

// Renders a standalone local .html/.htm file via QWebEngineView (a real,
// JavaScript-capable Chromium engine), unlike PdfView/EpubView which use
// Qt's lightweight, script-free QTextBrowser. That trade brings real
// HTML5/CSS3/JS support at the cost of some feature parity — WebEngine has
// no synchronous API for extracting page text or a QTextDocument-style
// selection model, so cross-document search and highlight annotations
// (which the other formats support) are not implemented here. See
// search()'s doc comment for specifics.
class HtmlView : public QWidget, public IReaderView
{
    Q_OBJECT

public:
    explicit HtmlView(const QString &filePath, QWidget *parent = nullptr);

    QString documentTitle() const override;
    QVector<TocNode> tableOfContents() const override; // always empty — a single file has no chapters
    void goToTocNode(const TocNode &node) override; // no-op, for the same reason
    int currentPosition() const override; // always 0

    // Always returns empty. WebEngine content lives in a separate renderer
    // process with no synchronous text-extraction API, so it can't satisfy
    // this method's synchronous contract the way PdfView/EpubView do. A
    // real "Find in page" bar (QWebEnginePage::findText, async, one-match-
    // at-a-time) would be the right fit for this format instead, as a
    // separate feature from the cross-document SearchDock.
    QVector<SearchResult> search(const QString &query) const override;
    void setSearchTerm(const QString &term) override; // no-op, see search() above
    void refreshHighlights() override { } // no highlight support, see class comment above

    qreal currentZoomFactor() const;

public slots:
    void zoomIn();
    void zoomOut();

private:
    void restoreZoom();
    void scheduleProgressSave();
    void saveProgressNow();

    QString m_filePath;
    QString m_fallbackTitle;
    QString m_bookHash;

    QWebEngineView *m_webView = nullptr;
    QLabel *m_titleLabel = nullptr;
    QTimer *m_progressSaveTimer = nullptr;
};
