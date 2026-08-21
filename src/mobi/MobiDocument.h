#pragma once

#include "core/Document.h" // TocNode

#include <QString>
#include <QVector>

#include <memory>

// MOBI/AZW/AZW3 support via libmobi (LGPLv3): reconstructs the archive's
// chapter-like "parts" as standalone HTML plus a nested table of contents
// from its NCX index — structurally the same shape as EPUB's spine + NCX
// (verified against a real KF8 sample: its NCX literally reads
// `<content src="part00000.html"/>`), so MobiView mirrors EpubView rather
// than the single-flowing-document model MarkdownView uses.
//
// DRM-protected files are refused outright — see load()'s doc comment.
// libmobi's decrypt functions (mobi_drm_decrypt() etc.) are never called
// anywhere in this codebase, deliberately: Mnemosyne reads files the user
// already has unencrypted access to, not a DRM-removal tool.
class MobiDocument
{
public:
    // Returns nullptr and fills errorMessage on failure — including when
    // the file is DRM-protected (checked via libmobi's mobi_is_encrypted()
    // before any parsing is attempted; never decrypted).
    static std::unique_ptr<MobiDocument> load(const QString &filePath, QString *errorMessage);

    QString title() const { return m_title; }
    int partCount() const { return m_partHtml.size(); }
    QString partHtml(int index) const { return m_partHtml.value(index); }

    // pageNumber is a part index, like EpubDocument's spine index — no
    // intra-chapter fragment precision, matching EPUB's own TocNode
    // contract (EpubSpineItem's href is fragment-stripped too).
    QVector<TocNode> tableOfContents() const { return m_toc; }

private:
    MobiDocument() = default;

    QString m_title;
    QVector<QString> m_partHtml;
    QVector<TocNode> m_toc;
};
