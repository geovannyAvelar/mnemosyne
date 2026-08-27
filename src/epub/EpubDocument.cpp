#include "EpubDocument.h"

#include "ZipArchive.h"
#include "core/HtmlAttrUtil.h"

#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <QUrl>
#include <QXmlStreamReader>

#include <algorithm>

namespace {

// See chapterHtml()'s image-embedding pass: an image wider than this gets
// explicit pixel width/height attributes scaling it down to fit, since
// QTextDocument doesn't honor CSS percentage sizing at all. Comfortably
// narrower than most of this app's reading pane widths without looking
// cramped on a narrower one.
constexpr int kMaxEmbeddedImageWidth = 720;

QString resolveEpubPath(const QString &baseDir, const QString &href)
{
    QString h = href;
    const int hashIndex = h.indexOf(QLatin1Char('#'));
    if (hashIndex >= 0) {
        h = h.left(hashIndex);
    }
    h = QUrl::fromPercentEncoding(h.toUtf8());

    if (h.startsWith(QLatin1Char('/'))) {
        h = h.mid(1);
    } else if (!baseDir.isEmpty()) {
        h = baseDir + QLatin1Char('/') + h;
    }

    // Manually collapse "." and ".." segments; archive entry paths always use
    // forward slashes regardless of host OS, so QDir::cleanPath (which is
    // platform-aware) is not a safe fit here.
    const QStringList inputSegments = h.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QStringList outputSegments;
    for (const QString &segment : inputSegments) {
        if (segment == QLatin1String(".")) {
            continue;
        }
        if (segment == QLatin1String("..")) {
            if (!outputSegments.isEmpty()) {
                outputSegments.removeLast();
            }
            continue;
        }
        outputSegments.append(segment);
    }
    return outputSegments.join(QLatin1Char('/'));
}

QString dirOf(const QString &path)
{
    const int slashIndex = path.lastIndexOf(QLatin1Char('/'));
    return slashIndex >= 0 ? path.left(slashIndex) : QString();
}

QString mimeTypeForImagePath(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String("jpg") || ext == QLatin1String("jpeg")) {
        return QStringLiteral("image/jpeg");
    }
    if (ext == QLatin1String("png")) {
        return QStringLiteral("image/png");
    }
    if (ext == QLatin1String("gif")) {
        return QStringLiteral("image/gif");
    }
    if (ext == QLatin1String("svg")) {
        return QStringLiteral("image/svg+xml");
    }
    if (ext == QLatin1String("webp")) {
        return QStringLiteral("image/webp");
    }
    return QStringLiteral("image/png");
}

// Matches a whole <video ...>...</video> block: group 1 is its own
// attributes (for a bare <video src="...">, no <source> children), group 2
// is its inner content (where <source> children live). DotMatchesEverything
// since a video element's inner content is free to span multiple lines.
const QRegularExpression kVideoTagRe(QStringLiteral("<video\\b([^>]*)>(.*?)</video\\s*>"),
                                      QRegularExpression::CaseInsensitiveOption
                                          | QRegularExpression::DotMatchesEverythingOption);

// Picks the <source> this app will actually try to play: video/mp4 when
// there's a choice (the most broadly supported format for an OS's default
// player), else the first <source>, else the <video> tag's own src
// attribute (the no-<source>-children form). Empty if none of those exist.
QString pickVideoSource(const QString &videoAttrs, const QString &videoInner)
{
    static const QRegularExpression sourceRe(QStringLiteral("<source\\b[^>]*>"), QRegularExpression::CaseInsensitiveOption);

    QString firstSrc;
    QString mp4Src;
    auto it = sourceRe.globalMatch(videoInner);
    while (it.hasNext()) {
        const QString tag = it.next().captured(0);
        const QString src = extractHtmlAttr(tag, QStringLiteral("src"));
        if (src.isEmpty()) {
            continue;
        }
        if (firstSrc.isEmpty()) {
            firstSrc = src;
        }
        if (mp4Src.isEmpty() && extractHtmlAttr(tag, QStringLiteral("type")).contains(QLatin1String("mp4"), Qt::CaseInsensitive)) {
            mp4Src = src;
        }
    }
    if (!mp4Src.isEmpty()) {
        return mp4Src;
    }
    if (!firstSrc.isEmpty()) {
        return firstSrc;
    }
    return extractHtmlAttr(videoAttrs, QStringLiteral("src"));
}

bool looksLikeIsbn13(const QString &s)
{
    if (s.size() != 13) {
        return false;
    }
    return std::all_of(s.begin(), s.end(), [](QChar c) { return c.isDigit(); });
}

bool looksLikeIsbn10(const QString &s)
{
    if (s.size() != 10) {
        return false;
    }
    for (int i = 0; i < 9; ++i) {
        if (!s.at(i).isDigit()) {
            return false;
        }
    }
    const QChar last = s.at(9);
    return last.isDigit() || last.toUpper() == QLatin1Char('X');
}

// dc:identifier has no fixed vocabulary -- an EPUB can carry a UUID, a DOI,
// an ASIN, and an ISBN side by side with no reliable way to tell which is
// which from the element alone (opf:scheme isn't required and is often
// omitted in practice). Scanning every candidate for one that reduces to a
// bare 10- or 13-digit ISBN shape is the pragmatic fallback: it's rare for
// a UUID/DOI to collapse into that exact shape once punctuation is
// stripped, so false positives are unlikely in practice.
QString isbnFromIdentifiers(const QStringList &candidates)
{
    for (const QString &raw : candidates) {
        QString cleaned;
        for (const QChar &c : raw) {
            if (c.isLetterOrNumber()) {
                cleaned.append(c.toUpper());
            }
        }
        if (cleaned.startsWith(QLatin1String("URNISBN"))) {
            cleaned.remove(0, 7);
        } else if (cleaned.startsWith(QLatin1String("ISBN"))) {
            cleaned.remove(0, 4);
        }
        if (looksLikeIsbn13(cleaned) || looksLikeIsbn10(cleaned)) {
            return cleaned;
        }
    }
    return {};
}

} // namespace

EpubDocument::EpubDocument() = default;
EpubDocument::~EpubDocument() = default;

std::unique_ptr<EpubDocument> EpubDocument::load(const QString &filePath, QString *errorMessage)
{
    auto doc = std::unique_ptr<EpubDocument>(new EpubDocument());

    doc->m_archive = ZipArchive::open(filePath, errorMessage);
    if (!doc->m_archive) {
        return nullptr;
    }

    QString opfPath;
    if (!doc->parseContainer(&opfPath, errorMessage)) {
        return nullptr;
    }

    if (!doc->parseOpf(opfPath, errorMessage)) {
        return nullptr;
    }

    if (doc->m_title.isEmpty()) {
        doc->m_title = QFileInfo(filePath).completeBaseName();
    }

    if (doc->m_spine.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("EPUB has no readable spine items: %1").arg(filePath);
        }
        return nullptr;
    }

    return doc;
}

bool EpubDocument::parseContainer(QString *opfPath, QString *errorMessage)
{
    bool ok = false;
    const QByteArray data = m_archive->readEntry(QStringLiteral("META-INF/container.xml"), &ok);
    if (!ok) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Missing META-INF/container.xml");
        }
        return false;
    }

    QXmlStreamReader reader(data);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name().compare(QLatin1String("rootfile"), Qt::CaseInsensitive) == 0) {
            *opfPath = reader.attributes().value(QLatin1String("full-path")).toString();
            return !opfPath->isEmpty();
        }
    }

    if (errorMessage) {
        *errorMessage = reader.hasError() ? reader.errorString() : QObject::tr("No rootfile found in container.xml");
    }
    return false;
}

bool EpubDocument::parseOpf(const QString &opfPath, QString *errorMessage)
{
    bool ok = false;
    const QByteArray data = m_archive->readEntry(opfPath, &ok);
    if (!ok) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Missing OPF file: %1").arg(opfPath);
        }
        return false;
    }

    m_opfDir = dirOf(opfPath);

    QHash<QString, QString> manifestIdToHref;
    QString navHref;
    QString ncxId;
    QStringList identifierCandidates;
    QString coverManifestId; // EPUB2 fallback: <meta name="cover" content="{manifest id}">
    QString coverHref;       // EPUB3: an <item> with properties="cover-image"

    QXmlStreamReader reader(data);
    bool insideMetadata = false;

    while (!reader.atEnd()) {
        const QXmlStreamReader::TokenType tok = reader.readNext();

        if (tok == QXmlStreamReader::StartElement) {
            const QString name = reader.name().toString();

            if (name.compare(QLatin1String("metadata"), Qt::CaseInsensitive) == 0) {
                insideMetadata = true;
            } else if (insideMetadata && name.compare(QLatin1String("title"), Qt::CaseInsensitive) == 0 && m_title.isEmpty()) {
                m_title = reader.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
            } else if (insideMetadata && name.compare(QLatin1String("identifier"), Qt::CaseInsensitive) == 0) {
                identifierCandidates.append(reader.readElementText(QXmlStreamReader::SkipChildElements).trimmed());
            } else if (insideMetadata && name.compare(QLatin1String("creator"), Qt::CaseInsensitive) == 0) {
                const QString author = reader.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                if (!author.isEmpty()) {
                    m_authors.append(author);
                }
            } else if (insideMetadata && name.compare(QLatin1String("publisher"), Qt::CaseInsensitive) == 0
                       && m_publisher.isEmpty()) {
                m_publisher = reader.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
            } else if (insideMetadata && name.compare(QLatin1String("description"), Qt::CaseInsensitive) == 0
                       && m_description.isEmpty()) {
                m_description = reader.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
            } else if (insideMetadata && name.compare(QLatin1String("meta"), Qt::CaseInsensitive) == 0) {
                const QXmlStreamAttributes attrs = reader.attributes();
                if (attrs.value(QLatin1String("name")).toString().compare(QLatin1String("cover"), Qt::CaseInsensitive) == 0) {
                    coverManifestId = attrs.value(QLatin1String("content")).toString();
                }
            } else if (name.compare(QLatin1String("item"), Qt::CaseInsensitive) == 0) {
                const QXmlStreamAttributes attrs = reader.attributes();
                const QString id = attrs.value(QLatin1String("id")).toString();
                const QString href = attrs.value(QLatin1String("href")).toString();
                const QString properties = attrs.value(QLatin1String("properties")).toString();
                if (!id.isEmpty() && !href.isEmpty()) {
                    const QString resolved = resolveEpubPath(m_opfDir, href);
                    manifestIdToHref.insert(id, resolved);
                    if (properties.contains(QLatin1String("nav"), Qt::CaseInsensitive)) {
                        navHref = resolved;
                    }
                    if (properties.contains(QLatin1String("cover-image"), Qt::CaseInsensitive)) {
                        coverHref = resolved;
                    }
                }
            } else if (name.compare(QLatin1String("itemref"), Qt::CaseInsensitive) == 0) {
                const QString idref = reader.attributes().value(QLatin1String("idref")).toString();
                const QString href = manifestIdToHref.value(idref);
                if (!href.isEmpty()) {
                    EpubSpineItem item;
                    item.href = href;
                    m_hrefToSpineIndex.insert(href, m_spine.size());
                    m_spine.append(item);
                }
            } else if (name.compare(QLatin1String("spine"), Qt::CaseInsensitive) == 0) {
                ncxId = reader.attributes().value(QLatin1String("toc")).toString();
            }
        } else if (tok == QXmlStreamReader::EndElement) {
            if (reader.name().compare(QLatin1String("metadata"), Qt::CaseInsensitive) == 0) {
                insideMetadata = false;
            }
        }
    }

    if (reader.hasError()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Failed to parse OPF: %1").arg(reader.errorString());
        }
        return false;
    }

    m_isbn = isbnFromIdentifiers(identifierCandidates);

    if (coverHref.isEmpty() && !coverManifestId.isEmpty()) {
        coverHref = manifestIdToHref.value(coverManifestId);
    }
    if (!coverHref.isEmpty()) {
        bool coverOk = false;
        const QByteArray coverData = m_archive->readEntry(coverHref, &coverOk);
        if (coverOk) {
            m_cover = QImage::fromData(coverData);
        }
    }

    if (!ncxId.isEmpty() && manifestIdToHref.contains(ncxId)) {
        parseNcx(manifestIdToHref.value(ncxId));
    } else if (!navHref.isEmpty()) {
        parseNav(navHref);
    }

    return true;
}

void EpubDocument::parseNcx(const QString &ncxPath)
{
    bool ok = false;
    const QByteArray data = m_archive->readEntry(ncxPath, &ok);
    if (!ok) {
        return;
    }

    const QString baseDir = dirOf(ncxPath);
    QXmlStreamReader reader(data);

    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name().compare(QLatin1String("navMap"), Qt::CaseInsensitive) == 0) {
            while (!reader.atEnd()) {
                const QXmlStreamReader::TokenType tok = reader.readNext();
                if (tok == QXmlStreamReader::EndElement && reader.name().compare(QLatin1String("navMap"), Qt::CaseInsensitive) == 0) {
                    break;
                }
                if (tok == QXmlStreamReader::StartElement && reader.name().compare(QLatin1String("navPoint"), Qt::CaseInsensitive) == 0) {
                    m_toc.append(parseNcxNavPoint(reader, baseDir));
                }
            }
            break;
        }
    }
}

TocNode EpubDocument::parseNcxNavPoint(QXmlStreamReader &reader, const QString &baseDir)
{
    // Precondition: reader is currently positioned at the navPoint's StartElement.
    TocNode node;

    while (!reader.atEnd() && !reader.hasError()) {
        const QXmlStreamReader::TokenType tok = reader.readNext();

        if (tok == QXmlStreamReader::EndElement && reader.name().compare(QLatin1String("navPoint"), Qt::CaseInsensitive) == 0) {
            break;
        }

        if (tok == QXmlStreamReader::StartElement) {
            const QString name = reader.name().toString();
            if (name.compare(QLatin1String("text"), Qt::CaseInsensitive) == 0 && node.title.isEmpty()) {
                node.title = reader.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
            } else if (name.compare(QLatin1String("content"), Qt::CaseInsensitive) == 0) {
                const QString src = reader.attributes().value(QLatin1String("src")).toString();
                node.pageNumber = spineIndexForHref(baseDir, src);
            } else if (name.compare(QLatin1String("navPoint"), Qt::CaseInsensitive) == 0) {
                node.children.append(parseNcxNavPoint(reader, baseDir));
            }
        }
    }

    return node;
}

void EpubDocument::parseNav(const QString &navPath)
{
    bool ok = false;
    const QByteArray data = m_archive->readEntry(navPath, &ok);
    if (!ok) {
        return;
    }

    const QString baseDir = dirOf(navPath);
    QXmlStreamReader reader(data);

    // Find <nav epub:type="toc">, then its first <ol>.
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement() || reader.name().compare(QLatin1String("nav"), Qt::CaseInsensitive) != 0) {
            continue;
        }

        bool isTocNav = false;
        const QXmlStreamAttributes attrs = reader.attributes();
        for (const QXmlStreamAttribute &attr : attrs) {
            if (attr.qualifiedName().toString().compare(QLatin1String("epub:type"), Qt::CaseInsensitive) == 0
                && attr.value().contains(QLatin1String("toc"), Qt::CaseInsensitive)) {
                isTocNav = true;
                break;
            }
        }
        if (!isTocNav) {
            continue;
        }

        while (!reader.atEnd()) {
            const QXmlStreamReader::TokenType tok = reader.readNext();
            if (tok == QXmlStreamReader::EndElement && reader.name().compare(QLatin1String("nav"), Qt::CaseInsensitive) == 0) {
                return;
            }
            if (tok == QXmlStreamReader::StartElement && reader.name().compare(QLatin1String("li"), Qt::CaseInsensitive) == 0) {
                m_toc.append(parseNavListItem(reader, baseDir));
            }
        }
    }
}

TocNode EpubDocument::parseNavListItem(QXmlStreamReader &reader, const QString &baseDir)
{
    // Precondition: reader is currently positioned at the <li>'s StartElement.
    TocNode node;

    while (!reader.atEnd() && !reader.hasError()) {
        const QXmlStreamReader::TokenType tok = reader.readNext();

        if (tok == QXmlStreamReader::EndElement && reader.name().compare(QLatin1String("li"), Qt::CaseInsensitive) == 0) {
            break;
        }

        if (tok == QXmlStreamReader::StartElement) {
            const QString name = reader.name().toString();
            if (name.compare(QLatin1String("a"), Qt::CaseInsensitive) == 0) {
                const QString href = reader.attributes().value(QLatin1String("href")).toString();
                if (!href.isEmpty()) {
                    node.pageNumber = spineIndexForHref(baseDir, href);
                }
                if (node.title.isEmpty()) {
                    node.title = reader.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                }
            } else if (name.compare(QLatin1String("ol"), Qt::CaseInsensitive) == 0) {
                while (!reader.atEnd()) {
                    const QXmlStreamReader::TokenType innerTok = reader.readNext();
                    if (innerTok == QXmlStreamReader::EndElement && reader.name().compare(QLatin1String("ol"), Qt::CaseInsensitive) == 0) {
                        break;
                    }
                    if (innerTok == QXmlStreamReader::StartElement && reader.name().compare(QLatin1String("li"), Qt::CaseInsensitive) == 0) {
                        node.children.append(parseNavListItem(reader, baseDir));
                    }
                }
            }
        }
    }

    return node;
}

int EpubDocument::spineIndexForHref(const QString &baseDir, const QString &href) const
{
    return m_hrefToSpineIndex.value(resolveEpubPath(baseDir, href), -1);
}

QString EpubDocument::chapterHtml(int spineIndex) const
{
    if (spineIndex < 0 || spineIndex >= m_spine.size()) {
        return {};
    }

    const QString href = m_spine.at(spineIndex).href;
    bool ok = false;
    const QByteArray raw = m_archive->readEntry(href, &ok);
    if (!ok) {
        return QStringLiteral("<p><i>%1</i></p>").arg(QObject::tr("Could not load chapter: %1").arg(href.toHtmlEscaped()));
    }

    const QString chapterDir = dirOf(href);
    QString html = QString::fromUtf8(raw);

    // Inline linked stylesheets as <style> blocks.
    {
        static const QRegularExpression linkRe(QStringLiteral("<link\\b[^>]*>"), QRegularExpression::CaseInsensitiveOption);
        QString transformed;
        transformed.reserve(html.size());
        int lastPos = 0;
        auto it = linkRe.globalMatch(html);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            const QString tag = m.captured(0);
            const QString rel = extractHtmlAttr(tag, QStringLiteral("rel"));

            transformed += html.mid(lastPos, m.capturedStart() - lastPos);
            lastPos = m.capturedEnd();

            if (rel.contains(QLatin1String("stylesheet"), Qt::CaseInsensitive)) {
                const QString cssHref = extractHtmlAttr(tag, QStringLiteral("href"));
                const QString cssPath = resolveEpubPath(chapterDir, cssHref);
                bool cssOk = false;
                const QByteArray css = m_archive->readEntry(cssPath, &cssOk);
                if (cssOk) {
                    transformed += QStringLiteral("<style>%1</style>").arg(QString::fromUtf8(css));
                }
            } else {
                transformed += tag; // not a stylesheet link, keep as-is
            }
        }
        transformed += html.mid(lastPos);
        html = transformed;
    }

    // Embed images as data: URIs.
    {
        static const QRegularExpression imgRe(QStringLiteral("<img\\b[^>]*>"), QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression srcRe(QStringLiteral("\\bsrc\\s*=\\s*(\"[^\"]*\"|'[^']*')"), QRegularExpression::CaseInsensitiveOption);

        QString transformed;
        transformed.reserve(html.size());
        int lastPos = 0;
        auto it = imgRe.globalMatch(html);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            QString tag = m.captured(0);
            const QString src = extractHtmlAttr(tag, QStringLiteral("src"));

            if (!src.isEmpty()) {
                const QString imgPath = resolveEpubPath(chapterDir, src);
                bool imgOk = false;
                const QByteArray imgData = m_archive->readEntry(imgPath, &imgOk);
                if (imgOk) {
                    const QString dataUri = QStringLiteral("data:%1;base64,%2").arg(mimeTypeForImagePath(imgPath), QString::fromLatin1(imgData.toBase64()));
                    tag.replace(srcRe, QStringLiteral("src=\"%1\"").arg(dataUri));

                    // QTextDocument (this app's EPUB renderer) doesn't honor
                    // percentage-based CSS sizing on <img> at all -- confirmed
                    // empirically: an <img style="max-width:100%"> (what a
                    // well-authored EPUB's own cover page often specifies)
                    // still renders at the image's native pixel size
                    // regardless, which for a full-bleed cover can be
                    // thousands of pixels wide, overflowing and clipping past
                    // the page's edge instead of scaling down as intended.
                    // Explicit pixel width/height attributes DO scale
                    // correctly (also confirmed empirically), so an oversized
                    // image gets capped to one here, computed from its own
                    // aspect ratio.
                    const QImage image = QImage::fromData(imgData);
                    if (!image.isNull() && image.width() > kMaxEmbeddedImageWidth) {
                        const int scaledHeight = (image.height() * kMaxEmbeddedImageWidth) / image.width();
                        static const QRegularExpression widthAttrRe(QStringLiteral("\\bwidth\\s*=\\s*(\"[^\"]*\"|'[^']*')"),
                                                                     QRegularExpression::CaseInsensitiveOption);
                        static const QRegularExpression heightAttrRe(QStringLiteral("\\bheight\\s*=\\s*(\"[^\"]*\"|'[^']*')"),
                                                                      QRegularExpression::CaseInsensitiveOption);
                        tag.remove(widthAttrRe);
                        tag.remove(heightAttrRe);
                        // Position 4, not a literal "<img" replace: the tag
                        // may be spelled "<IMG" (the opening regex match is
                        // case-insensitive), but either way it's exactly 4
                        // characters before here.
                        tag.insert(4, QStringLiteral(" width=\"%1\" height=\"%2\"").arg(kMaxEmbeddedImageWidth).arg(scaledHeight));
                    }
                }
            }

            transformed += html.mid(lastPos, m.capturedStart() - lastPos);
            transformed += tag;
            lastPos = m.capturedEnd();
        }
        transformed += html.mid(lastPos);
        html = transformed;
    }

    // Replace <video> elements with a "Play Video" link: QTextBrowser has no
    // video support at all (the tag would just silently vanish), so this at
    // least gives the reader an obvious way to watch it. EpubView resolves
    // "mnemosyne-video:N" to the Nth entry of chapterVideoPaths() for this
    // same chapter, extracts it to a temp file, and hands that to the OS's
    // default player -- a real in-app player isn't practical without
    // switching chapter rendering to a full web engine.
    {
        QString transformed;
        transformed.reserve(html.size());
        int lastPos = 0;
        int videoIndex = 0;
        auto it = kVideoTagRe.globalMatch(html);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            const QString src = pickVideoSource(m.captured(1), m.captured(2));

            transformed += html.mid(lastPos, m.capturedStart() - lastPos);
            if (!src.isEmpty()) {
                const QString label = QFileInfo(src).fileName().toHtmlEscaped();
                // An explicit color, not left to inherit from the book's own
                // CSS (or default rich-text link styling): QTextBrowser's
                // dark-mode override (see EpubView::renderCurrentChapter())
                // only lightens body text, not anchors, so an unstyled link
                // here rendered in the book's own dark body-text color --
                // nearly invisible against the dark-mode page background.
                // #D97756 is Theme's accent color, chosen because it's the
                // same hex in both the light and dark palette.
                transformed += QStringLiteral("<p><a href=\"mnemosyne-video:%1\" style=\"color:#D97756;\">&#9654; %2</a></p>")
                                   .arg(videoIndex)
                                   .arg(label);
            }
            // else: no usable source at all, drop the element silently --
            // same treatment an <img> with no src (or a broken one) gets.
            lastPos = m.capturedEnd();
            ++videoIndex; // keeps this in lockstep with chapterVideoPaths()'s indexing regardless of src
        }
        transformed += html.mid(lastPos);
        html = transformed;
    }

    return html;
}

QVector<QString> EpubDocument::chapterVideoPaths(int spineIndex) const
{
    QVector<QString> paths;
    if (spineIndex < 0 || spineIndex >= m_spine.size()) {
        return paths;
    }

    const QString href = m_spine.at(spineIndex).href;
    bool ok = false;
    const QByteArray raw = m_archive->readEntry(href, &ok);
    if (!ok) {
        return paths;
    }

    const QString chapterDir = dirOf(href);
    const QString html = QString::fromUtf8(raw);

    auto it = kVideoTagRe.globalMatch(html);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const QString src = pickVideoSource(m.captured(1), m.captured(2));
        paths.append(src.isEmpty() ? QString() : resolveEpubPath(chapterDir, src));
    }
    return paths;
}

QByteArray EpubDocument::readResource(const QString &archivePath, bool *ok) const
{
    return m_archive->readEntry(archivePath, ok);
}
