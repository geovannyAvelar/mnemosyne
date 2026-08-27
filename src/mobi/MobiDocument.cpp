#include "MobiDocument.h"

#include <mobi.h>

#include <QFileInfo>
#include <QHash>
#include <QObject>
#include <QRegularExpression>

#include <cstdint>
#include <cstdlib>

namespace {

// NCX index tag ids, straight from the Mobipocket/KF8 index format itself
// (mirrored from libmobi's INDX_TAG_NCX_* constants in its internal,
// not-installed src/index.h) — documented file-format constants, not
// libmobi implementation details, so hardcoding them here doesn't couple
// this to a specific libmobi version.
constexpr unsigned kTagTextCncx = 3; // offset into the CNCX string table, value index 0
constexpr unsigned kTagLevel = 4;
constexpr unsigned kTagPosFid = 6; // value index 0

bool tagValue(const MOBIIndexEntry &entry, unsigned tagId, unsigned valueIndex, uint32_t *out)
{
    for (size_t i = 0; i < entry.tags_count; ++i) {
        const MOBIIndexTag &tag = entry.tags[i];
        if (tag.tagid == tagId && valueIndex < tag.tagvalues_count) {
            *out = tag.tagvalues[valueIndex];
            return true;
        }
    }
    return false;
}

QString takeMobiString(char *raw)
{
    const QString result = raw ? QString::fromUtf8(raw) : QString();
    free(raw); // libmobi's mobi_meta_get_*() return heap strings the caller owns
    return result;
}

// mobi_meta_get_isbn() returns the EXTH ISBN record verbatim, which in
// practice sometimes carries hyphens (e.g. "978-0-13-468599-1") -- strip
// down to the bare digits/checksum letter so this matches the normalized
// shape EpubDocument::isbn() produces, ready for an ISBN-keyed lookup.
QString normalizeIsbn(const QString &raw)
{
    QString cleaned;
    for (const QChar &c : raw) {
        if (c.isLetterOrNumber()) {
            cleaned.append(c.toUpper());
        }
    }
    const bool isIsbn13 = cleaned.size() == 13;
    const bool isIsbn10 = cleaned.size() == 10;
    if (!isIsbn13 && !isIsbn10) {
        return {};
    }
    return cleaned;
}

// EXTH's author record is a single string; multiple authors show up as one
// value joined with "&" or ";" (no fixed separator in the format), so split
// on either rather than treating the whole thing as one name.
QStringList splitAuthors(const QString &raw)
{
    static const QRegularExpression separator(QStringLiteral("\\s*[;&]\\s*"));
    QStringList authors;
    for (const QString &author : raw.split(separator, Qt::SkipEmptyParts)) {
        const QString trimmed = author.trimmed();
        if (!trimmed.isEmpty()) {
            authors.append(trimmed);
        }
    }
    return authors;
}

// KindleGen's embedded cover: EXTH_COVEROFFSET names an index, relative to
// the first resource record, into the PDB record list; that record holds
// the raw image bytes directly (no further container format wraps it).
QImage extractCover(const MOBIData *m)
{
    MOBIExthHeader *exth = mobi_get_exthrecord_by_tag(m, EXTH_COVEROFFSET);
    if (!exth || !exth->data) {
        return {};
    }
    const uint32_t offset = mobi_decode_exthvalue(static_cast<const unsigned char *>(exth->data), exth->size);

    const size_t firstResource = mobi_get_first_resource_record(m);
    if (firstResource == MOBI_NOTSET) {
        return {};
    }

    const MOBIPdbRecord *record = mobi_get_record_by_seqnumber(m, firstResource + offset);
    if (!record || !record->data) {
        return {};
    }
    return QImage::fromData(record->data, static_cast<int>(record->size));
}

// CP1252 matches Latin-1 one-for-one except for 0x80-0x9F, which CP1252
// assigns to these printable characters (mostly smart quotes/dashes)
// instead of the C1 control codes Latin-1 leaves there. A fixed, standard
// mapping table, not something libmobi-specific.
QChar decodeCp1252Byte(unsigned char byte)
{
    static const char16_t kHighRange[32] = {
        0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
        0x2039, 0x0152, 0x008D, 0x017D, 0x008F, 0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
        0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
    };
    if (byte >= 0x80 && byte <= 0x9F) {
        return QChar(kHighRange[byte - 0x80]);
    }
    return QChar(static_cast<char16_t>(byte));
}

// Reads a length-prefixed string from a "compiled NCX" (CNCX) record at the
// given byte offset — the varint-then-bytes format libmobi's own
// (internal, not-installed) mobi_get_cncx_string() decodes, reimplemented
// here against MOBIPdbRecord's public data/size fields.
QString cncxString(const MOBIPdbRecord *cncxRecord, uint32_t offset, MOBIEncoding encoding)
{
    if (!cncxRecord || !cncxRecord->data || offset >= cncxRecord->size) {
        return QString();
    }

    const unsigned char *data = cncxRecord->data;
    const size_t maxLen = cncxRecord->size;

    // Variable-length integer: up to 4 bytes, 7 payload bits each; the byte
    // with the 0x80 bit set is the last one.
    uint32_t stringLength = 0;
    size_t pos = offset;
    for (int i = 0; i < 4 && pos < maxLen; ++i) {
        const unsigned char byte = data[pos++];
        stringLength = (stringLength << 7) | (byte & 0x7f);
        if (byte & 0x80) {
            break;
        }
    }
    if (pos + stringLength > maxLen) {
        stringLength = static_cast<uint32_t>(maxLen - pos); // guard a corrupt/truncated record
    }

    if (encoding == MOBI_CP1252) {
        QString result;
        result.reserve(static_cast<int>(stringLength));
        for (uint32_t i = 0; i < stringLength; ++i) {
            result.append(decodeCp1252Byte(data[pos + i]));
        }
        return result;
    }
    return QString::fromUtf8(reinterpret_cast<const char *>(data + pos), static_cast<int>(stringLength));
}

// Mirrors MarkdownDocument::buildTableOfContents()'s open-node-stack
// algorithm: NCX entries arrive in document order with an explicit nesting
// level (rather than one inferred from "#" counts), so the same approach
// applies unchanged.
QVector<TocNode> buildTableOfContents(const MOBIIndx *ncx, const QHash<size_t, int> &partIndexByUid)
{
    QVector<TocNode> roots;
    if (!ncx) {
        return roots;
    }

    struct OpenNode
    {
        int level;
        TocNode node;
    };
    QVector<OpenNode> open;

    auto closeDownTo = [&](int level) {
        while (!open.isEmpty() && open.last().level >= level) {
            const TocNode finished = open.takeLast().node;
            if (!open.isEmpty()) {
                open.last().node.children.append(finished);
            } else {
                roots.append(finished);
            }
        }
    };

    for (size_t i = 0; i < ncx->entries_count; ++i) {
        const MOBIIndexEntry &entry = ncx->entries[i];

        uint32_t levelValue = 0;
        tagValue(entry, kTagLevel, 0, &levelValue);
        closeDownTo(static_cast<int>(levelValue));

        // -1 (matching EPUB's TocNode::pageNumber "unknown" convention) for
        // entries addressed by absolute file offset instead of part id —
        // an older KF7-style addressing scheme this doesn't resolve.
        int targetPart = -1;
        uint32_t posfid = 0;
        if (tagValue(entry, kTagPosFid, 0, &posfid)) {
            targetPart = partIndexByUid.value(posfid, -1);
        }

        QString title;
        uint32_t textCncxOffset = 0;
        if (tagValue(entry, kTagTextCncx, 0, &textCncxOffset)) {
            title = cncxString(ncx->cncx_record, textCncxOffset, ncx->encoding);
        }
        if (title.isEmpty()) {
            title = QObject::tr("(untitled)");
        }

        TocNode node;
        node.title = title;
        node.pageNumber = targetPart;
        open.append({static_cast<int>(levelValue), node});
    }
    closeDownTo(0);

    return roots;
}

} // namespace

std::unique_ptr<MobiDocument> MobiDocument::load(const QString &filePath, QString *errorMessage)
{
    MOBIData *m = mobi_init();
    if (!m) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Out of memory");
        }
        return nullptr;
    }

    const QByteArray pathUtf8 = filePath.toUtf8();
    if (mobi_load_filename(m, pathUtf8.constData()) != MOBI_SUCCESS) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Could not open file: %1").arg(filePath);
        }
        mobi_free(m);
        return nullptr;
    }

    // Hard refusal, not a "decrypt with a key" prompt — see MobiDocument.h.
    // libmobi's mobi_drm_decrypt() is never called anywhere in this file.
    if (mobi_is_encrypted(m)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("This file is DRM-protected and can't be opened.");
        }
        mobi_free(m);
        return nullptr;
    }

    MOBIRawml *rawml = mobi_init_rawml(m);
    if (!rawml) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Out of memory");
        }
        mobi_free(m);
        return nullptr;
    }

    if (mobi_parse_rawml(rawml, m) != MOBI_SUCCESS) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Could not parse file: %1").arg(filePath);
        }
        mobi_free_rawml(rawml);
        mobi_free(m);
        return nullptr;
    }

    auto document = std::unique_ptr<MobiDocument>(new MobiDocument());

    document->m_title = takeMobiString(mobi_meta_get_title(m));
    if (document->m_title.isEmpty()) {
        document->m_title = QFileInfo(filePath).completeBaseName();
    }
    document->m_isbn = normalizeIsbn(takeMobiString(mobi_meta_get_isbn(m)));
    document->m_authors = splitAuthors(takeMobiString(mobi_meta_get_author(m)));
    document->m_publisher = takeMobiString(mobi_meta_get_publisher(m));
    document->m_description = takeMobiString(mobi_meta_get_description(m));
    document->m_cover = extractCover(m);

    // rawml->markup is the linked list of reconstructed chapter-like HTML
    // parts (as opposed to ->flow for CSS and ->resources for images/OPF).
    QHash<size_t, int> partIndexByUid;
    for (MOBIPart *part = rawml->markup; part; part = part->next) {
        partIndexByUid.insert(part->uid, document->m_partHtml.size());
        document->m_partHtml.append(
            QString::fromUtf8(reinterpret_cast<const char *>(part->data), static_cast<int>(part->size)));
    }

    document->m_toc = buildTableOfContents(rawml->ncx, partIndexByUid);

    mobi_free_rawml(rawml);
    mobi_free(m);
    return document;
}
