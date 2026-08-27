#pragma once

#include "core/Document.h" // TocNode

#include <QHash>
#include <QImage>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

class ZipArchive;
class QXmlStreamReader;

struct EpubSpineItem
{
    QString href; // resolved path within archive, fragment stripped
    QString mediaType;
};

class EpubDocument
{
public:
    ~EpubDocument();

    // Returns nullptr and fills errorMessage on failure.
    static std::unique_ptr<EpubDocument> load(const QString &filePath, QString *errorMessage);

    QString title() const { return m_title; }

    // Normalized to bare digits (plus a possible trailing 'X' for ISBN-10),
    // no hyphens/prefixes -- ready to hand to an ISBN-keyed lookup. Empty
    // if the OPF's dc:identifier entries don't contain anything ISBN-shaped.
    QString isbn() const { return m_isbn; }

    // Local metadata straight from the OPF -- available even with no ISBN
    // and no network access; see BookInfoDock, which shows this immediately
    // and layers an Open Library lookup on top only when isbn() is set and
    // the user has opted into that.
    QStringList authors() const { return m_authors; }
    QString publisher() const { return m_publisher; }
    QString description() const { return m_description; }

    // The manifest's designated cover image (EPUB3 "cover-image" property,
    // falling back to EPUB2's <meta name="cover">), decoded from the
    // archive. Null if the OPF doesn't point to one or it fails to decode.
    QImage cover() const { return m_cover; }

    int spineCount() const { return m_spine.size(); }
    const EpubSpineItem &spineItem(int index) const { return m_spine.at(index); }
    QVector<TocNode> tableOfContents() const { return m_toc; }

    // Chapter XHTML transformed into standalone HTML: linked stylesheets are
    // inlined as <style> blocks and images are embedded as data: URIs, so
    // QTextBrowser can render it without needing to resolve archive-relative
    // resources itself.
    QString chapterHtml(int spineIndex) const;

    // Resolves an href (possibly with a #fragment), relative to baseDir, to a
    // spine index. Returns -1 if it doesn't match any spine item.
    int spineIndexForHref(const QString &baseDir, const QString &href) const;

private:
    EpubDocument();

    bool parseContainer(QString *opfPath, QString *errorMessage);
    bool parseOpf(const QString &opfPath, QString *errorMessage);
    void parseNcx(const QString &ncxPath);
    void parseNav(const QString &navPath);
    TocNode parseNcxNavPoint(QXmlStreamReader &reader, const QString &baseDir);
    TocNode parseNavListItem(QXmlStreamReader &reader, const QString &baseDir);

    std::unique_ptr<ZipArchive> m_archive;
    QString m_opfDir;
    QString m_title;
    QString m_isbn;
    QStringList m_authors;
    QString m_publisher;
    QString m_description;
    QImage m_cover;
    QVector<EpubSpineItem> m_spine;
    QHash<QString, int> m_hrefToSpineIndex;
    QVector<TocNode> m_toc;
};
