#pragma once

#include "core/Document.h" // TocNode

#include <QHash>
#include <QString>
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
    QVector<EpubSpineItem> m_spine;
    QHash<QString, int> m_hrefToSpineIndex;
    QVector<TocNode> m_toc;
};
