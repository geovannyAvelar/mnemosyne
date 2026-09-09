#pragma once

#include "PdfHighlightController.h"
#include "PdfSearchController.h"
#include "core/Document.h"
#include "core/PdfFormField.h"
#include "core/ReaderView.h"

#include <QWidget>

#include <memory>

class QKeyEvent;
class QLabel;
class QScrollArea;
class QSpinBox;
class QTimer;
class PdfPageStackView;
class ReadingProgressController;
class SyncPromptBar;

class PdfView : public QWidget, public IReaderView
{
    Q_OBJECT

public:
    // password is whatever unlocked `document`, if it was encrypted (empty
    // otherwise) -- kept only to re-open the same file for search() below,
    // never persisted anywhere.
    explicit PdfView(std::unique_ptr<IDocument> document, QString filePath, QWidget *parent = nullptr,
                      QString password = QString());
    ~PdfView() override;

    QString documentTitle() const override;
    QVector<TocNode> tableOfContents() const override;
    void goToTocNode(const TocNode &node) override;
    int currentPosition() const override;
    QVector<SearchResult> search(const QString &query) const override;
    void setSearchTerm(const QString &term) override;
    void refreshHighlights() override;
    void flushProgress() override;

    // Same search as above, but independent of any PdfView instance: opens
    // its own Poppler document from filePath rather than touching a live
    // view's m_document. Poppler documents aren't safe to use concurrently
    // from multiple threads, so this is what MainWindow calls on a
    // background thread while the view (and its m_document, used by the
    // main thread for rendering) keeps running independently -- including
    // for the search dock's search-the-current-tab feature, which is why
    // MainWindow fetches password() (below) for the active tab and passes
    // it through rather than leaving it empty: this file has already been
    // opened and unlocked once, so the reader shouldn't have to enter its
    // password a second time just to search within it.
    static QVector<SearchResult> searchFile(const QString &filePath, const QString &query,
                                             const QString &password = QString());

    QString selectedText() const;

    // AcroForm field filling (see core/PdfFormField.h, PopplerPdfDocument).
    // Forwards straight to the live PopplerPdfDocument this view owns --
    // unlike highlights/ink, a form field's value is written into the
    // document object itself, not a Mnemosyne-only sidecar; it only reaches
    // an actual file on disk via saveFilledFormAs() below.
    QVector<PdfFormField> formFields() const;
    void setFormFieldText(int pageIndex, int fieldIndex, const QString &value);
    void setFormFieldChecked(int pageIndex, int fieldIndex, bool checked);
    void setFormFieldChoiceIndex(int pageIndex, int fieldIndex, int choiceIndex);
    // Writes every field value set above into a new PDF at outputPath,
    // never overwriting the file this view was opened from. See
    // PopplerPdfDocument::saveFilledFormAs().
    bool saveFilledFormAs(const QString &outputPath) const;

    // Writes this book's highlights and ink strokes into a new PDF at
    // outputPath as real PDF annotations (see
    // PopplerPdfDocument::exportAnnotated()) -- visible and printable in
    // any PDF reader, unlike Mnemosyne's own app-side overlay of the same
    // marks. Never overwrites the file this view was opened from.
    bool exportAnnotatedAs(const QString &outputPath) const;

    bool hasPendingSyncPrompt() const;
    // Whatever unlocked this view's file, if it was encrypted; empty
    // otherwise. See searchFile()'s own doc comment for the one thing
    // outside this class that needs it.
    QString password() const { return m_password; }

    // Page color inversion -- distinct from the app-wide Dark Mode menu
    // action (which only restyles the UI chrome/EPUB-MOBI-TXT-Markdown
    // text; see MainWindow::setDarkModeEnabled()). PDF pages are rasterized
    // by Poppler with a fixed white background baked in, so there's no
    // stylesheet to swap -- inverting the rendered image (see
    // PdfPageStackView::setInvertColors()) is the only way to darken one.
    // Persisted under its own "pdfPageInvertColors" QSettings key (shared
    // with the Qt Quick side's ThemeSettings::pdfPageDark), separate from
    // "darkMode" so it doesn't follow the app theme. Controlled from
    // MainWindow's top bar (see updatePdfToolbarActions()), not a button of
    // this view's own toolbar.
    bool invertColors() const;
    void setInvertColors(bool enabled);

    // Two pages side by side, like an open book -- a layout toggle only
    // (see PdfPageStackView::setTwoPageMode()), not a separate reading mode:
    // scrolling/zoom/search/highlights/draw all keep working unchanged.
    // Persisted the same way as invertColors() above (a plain QSettings
    // key, not per-book).
    bool twoPageMode() const;
    void setTwoPageMode(bool enabled);

    // Whether toggleDrawMode(true) is currently in effect -- MainWindow
    // reads this to sync its Draw action's checked state on tab switch.
    bool drawMode() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

public slots:
    void goToPage(int index); // 0-based
    void nextPage();
    void previousPage();
    void zoomIn();
    void zoomOut();
    void copySelection();
    void addHighlightForSelection();
    void addNoteForSelection();
    // Toggles freehand-pen draw mode on the canvas (see
    // PdfPageStackView::setDrawMode()) -- checked state owned by
    // MainWindow's top-bar "Draw" action.
    void toggleDrawMode(bool enabled);
    // Erases every ink stroke on the current page (InkStore::clearPage()).
    void clearPageDrawings();

signals:
    // Emitted whenever this view adds, edits, or removes a highlight/note,
    // so MainWindow can keep the Notes dock in sync without polling.
    void highlightsChanged();

private:
    void setupUi();

    // The topmost page substantially visible in the viewport right now —
    // this is what m_currentPage tracks continuously while scrolling.
    int topmostVisiblePage() const;
    void onScrolled();

    void setZoom(qreal newZoom);
    void updateNavigationState();
    void showCanvasContextMenu(const QPoint &globalPos, int pageIndex, const QPointF &pagePoint);
    // A plain click (no drag) landed at pagePoint on pageIndex -- shows a
    // NotePopup if it hit an existing highlight that has a note. See
    // PdfPageStackView::clicked()'s own doc comment.
    void showNotePopupIfClickedOnNote(int pageIndex, const QPointF &pagePoint, const QPoint &globalPos);
    // Emitted by PdfPageStackView when a draw-mode drag commits (>= 2
    // points) -- persists it (InkStore) then feeds the updated stroke list
    // back into the canvas, the same round-trip addHighlightForSelection()
    // already does for highlights.
    void onInkStrokeDrawn(int pageIndex, const QVector<QPointF> &pagePoints);

    std::unique_ptr<IDocument> m_document;
    QString m_filePath;
    QString m_password; // empty unless m_filePath needed one to open -- see the constructor's own doc comment
    int m_currentPage = 0; // topmost substantially-visible page
    qreal m_zoom = 1.5;

    PdfHighlightController m_highlightController;
    PdfSearchController m_searchController;
    ReadingProgressController *m_progressController = nullptr;

    PdfPageStackView *m_pageStackView = nullptr; // the scroll area's content widget
    QScrollArea *m_scrollArea = nullptr;
    QSpinBox *m_pageSpinBox = nullptr;
    QLabel *m_pageCountLabel = nullptr;
    SyncPromptBar *m_syncPromptBar = nullptr;
};
