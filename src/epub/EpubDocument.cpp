#include "EpubDocument.h"

#include "ZipArchive.h"
#include "core/HtmlAttrUtil.h"

#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <QUrl>
#include <QXmlStreamReader>

namespace {

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
                }
            }

            transformed += html.mid(lastPos, m.capturedStart() - lastPos);
            transformed += tag;
            lastPos = m.capturedEnd();
        }
        transformed += html.mid(lastPos);
        html = transformed;
    }

    return html;
}
