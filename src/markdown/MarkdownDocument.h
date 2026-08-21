#pragma once

#include "core/Document.h" // TocNode

#include <QString>
#include <QVector>

#include <memory>

// A Markdown file is a single flat document — unlike EPUB there's no spine
// of chapters, so this holds the raw text plus a heading-derived table of
// contents instead of a per-chapter API. Rendering itself is delegated
// entirely to Qt's own Markdown-to-rich-text support
// (QTextDocument::setMarkdown(), in ui/MarkdownView) rather than duplicated
// here.
class MarkdownDocument
{
public:
    // Returns nullptr and fills errorMessage on failure.
    static std::unique_ptr<MarkdownDocument> load(const QString &filePath, QString *errorMessage);

    QString title() const { return m_title; }
    QString markdownText() const { return m_markdownText; }

    // Derived from ATX ("# Heading") lines outside fenced code blocks.
    // pageNumber on each node is the heading's 0-based ordinal among ALL
    // headings in the document, in the order they appear — MarkdownView
    // resolves this against the rendered QTextDocument's heading blocks
    // (see QTextBlockFormat::headingLevel()), which Qt's Markdown importer
    // preserves in the same order.
    QVector<TocNode> tableOfContents() const { return m_toc; }

    // The document's text sliced at heading boundaries, flat (not nested),
    // for per-section search results. headingIndex is -1 for the text
    // before the first heading, otherwise matches a tableOfContents() node's
    // pageNumber.
    struct Section
    {
        int headingIndex = -1;
        QString label;
        QString text;
    };
    QVector<Section> sections() const;

private:
    MarkdownDocument() = default;

    QString m_title;
    QString m_markdownText;
    QVector<TocNode> m_toc;
};
