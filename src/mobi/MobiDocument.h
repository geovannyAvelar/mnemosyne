#pragma once

#include "core/Document.h" // TocNode

#include <QImage>
#include <QString>
#include <QStringList>
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

    // Normalized to bare digits (plus a possible trailing 'X' for ISBN-10),
    // no hyphens -- ready to hand to an ISBN-keyed lookup. Empty if the
    // file's EXTH records don't carry one.
    QString isbn() const { return m_isbn; }

    // Local metadata straight from the file's EXTH records -- available
    // even with no ISBN and no network access; see BookInfoDock, which
    // shows this immediately and layers an Open Library lookup on top only
    // when isbn() is set and the user has opted into that.
    QStringList authors() const { return m_authors; }
    QString publisher() const { return m_publisher; }
    QString description() const { return m_description; }

    // The EXTH-designated cover resource, decoded. Null if the file has no
    // EXTH_COVEROFFSET record or it fails to decode.
    QImage cover() const { return m_cover; }

    int partCount() const { return m_partHtml.size(); }
    QString partHtml(int index) const { return m_partHtml.value(index); }

    // pageNumber is a part index, like EpubDocument's spine index — no
    // intra-chapter fragment precision, matching EPUB's own TocNode
    // contract (EpubSpineItem's href is fragment-stripped too).
    QVector<TocNode> tableOfContents() const { return m_toc; }

private:
    MobiDocument() = default;

    QString m_title;
    QString m_isbn;
    QStringList m_authors;
    QString m_publisher;
    QString m_description;
    QImage m_cover;
    QVector<QString> m_partHtml;
    QVector<TocNode> m_toc;
};
