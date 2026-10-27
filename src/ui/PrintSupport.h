#pragma once

#include <memory>

class IDocument;
class QPrinter;
class QTextDocument;

// Implemented by reader views that can send their content to a printer.
// MainWindow::printCurrent() runs the print dialog and hands the configured
// printer to whichever view is active.
class IPrintableView
{
public:
    virtual ~IPrintableView() = default;

    // Page count the print dialog should offer as a from/to range, or 0 for
    // formats that flow (EPUB/MOBI/Markdown/TXT print the text currently
    // loaded in the view, paginated by Qt).
    virtual int printablePageCount() const = 0;

    // Takes shared ownership because QtWebEngine prints asynchronously and
    // needs the printer to outlive this call.
    virtual void print(std::shared_ptr<QPrinter> printer) = 0;
};

namespace PrintSupport {

// Raster pages (PDF, CBZ): each page is rendered at the printer's
// resolution, scaled to fit the printable area and centered. Honors the
// from/to range chosen in the print dialog. Returns false if the printer
// could not be opened.
bool printPages(QPrinter *printer, const IDocument &document);

// Flowing text (EPUB/MOBI/Markdown/TXT): prints a copy of the document, so
// pagination for the printer never disturbs the on-screen layout.
bool printTextDocument(QPrinter *printer, const QTextDocument &document);

} // namespace PrintSupport
