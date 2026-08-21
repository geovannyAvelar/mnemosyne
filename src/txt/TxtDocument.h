#pragma once

#include <QString>

#include <memory>

// A plain-text file: no markup, no headings, no chapters — just the raw
// text. TxtView (unlike MarkdownView/MobiView) has no structure to derive a
// table of contents or per-section search results from, so navigation and
// search results both address the document by raw character offset instead.
class TxtDocument
{
public:
    // Returns nullptr and fills errorMessage on failure.
    static std::unique_ptr<TxtDocument> load(const QString &filePath, QString *errorMessage);

    QString title() const { return m_title; }
    QString text() const { return m_text; }

private:
    TxtDocument() = default;

    QString m_title;
    QString m_text;
};
