#include "HighlightStore.h"

#include "HighlightSync.h"

#include <QCryptographicHash>
#include <QSettings>
#include <QUuid>

#include <algorithm>

namespace {

QString groupKeyForHighlights(const QString &bookHash)
{
    const QByteArray hash = QCryptographicHash::hash(bookHash.toUtf8(), QCryptographicHash::Md5).toHex();
    return QStringLiteral("Highlights/%1").arg(QString::fromLatin1(hash));
}

void writeHighlights(const QString &bookHash, const QVector<Highlight> &highlights)
{
    QSettings settings;
    const QString group = groupKeyForHighlights(bookHash);
    settings.remove(group);
    settings.beginWriteArray(group);
    for (int i = 0; i < highlights.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("targetIndex", highlights[i].targetIndex);
        settings.setValue("rectX", highlights[i].pageRect.x());
        settings.setValue("rectY", highlights[i].pageRect.y());
        settings.setValue("rectW", highlights[i].pageRect.width());
        settings.setValue("rectH", highlights[i].pageRect.height());
        settings.setValue("text", highlights[i].text);
        settings.setValue("createdAt", highlights[i].createdAt);
        settings.setValue("note", highlights[i].note);
        settings.setValue("color", highlights[i].color.rgba());
        settings.setValue("id", highlights[i].id);
        settings.setValue("updatedAt", highlights[i].updatedAt);
    }
    settings.endArray();
}

void sortByTarget(QVector<Highlight> &highlights)
{
    std::sort(highlights.begin(), highlights.end(), [](const Highlight &a, const Highlight &b) {
        return a.targetIndex < b.targetIndex;
    });
}

} // namespace

namespace HighlightStore {

QVector<Highlight> highlightsFor(const QString &bookHash)
{
    QSettings settings;
    QVector<Highlight> result;
    bool needsRewrite = false;
    const int size = settings.beginReadArray(groupKeyForHighlights(bookHash));
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        Highlight h;
        h.targetIndex = settings.value("targetIndex", -1).toInt();
        const double x = settings.value("rectX", 0.0).toDouble();
        const double y = settings.value("rectY", 0.0).toDouble();
        const double w = settings.value("rectW", 0.0).toDouble();
        const double hgt = settings.value("rectH", 0.0).toDouble();
        if (w > 0 && hgt > 0) {
            h.pageRect = QRectF(x, y, w, hgt);
        }
        h.text = settings.value("text").toString();
        h.createdAt = settings.value("createdAt").toDateTime();
        h.note = settings.value("note").toString();
        h.color = QColor::fromRgba(settings.value("color", kDefaultHighlightColor.rgba()).toUInt());
        h.id = settings.value("id").toString();
        h.updatedAt = settings.value("updatedAt").toDateTime();

        // Data persisted before highlight sync existed has no id/updatedAt.
        // Mint them now so this highlight becomes syncable going forward,
        // instead of silently never reaching the user's other devices.
        if (h.id.isEmpty()) {
            h.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            h.updatedAt = h.createdAt;
            needsRewrite = true;
        }

        result.append(h);
    }
    settings.endArray();

    sortByTarget(result);

    if (needsRewrite) {
        writeHighlights(bookHash, result);
        for (const Highlight &h : result) {
            HighlightSync::pushUpsert(bookHash, h);
        }
    }

    return result;
}

void addHighlight(const QString &bookHash, const Highlight &highlightIn)
{
    Highlight highlight = highlightIn;
    highlight.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    highlight.updatedAt = QDateTime::currentDateTime();

    QVector<Highlight> highlights = highlightsFor(bookHash);
    highlights.append(highlight);
    sortByTarget(highlights);
    writeHighlights(bookHash, highlights);

    HighlightSync::pushUpsert(bookHash, highlight);
}

void removeHighlight(const QString &bookHash, int index)
{
    QVector<Highlight> highlights = highlightsFor(bookHash);
    if (index < 0 || index >= highlights.size()) {
        return;
    }
    const QString id = highlights[index].id;
    highlights.removeAt(index);
    writeHighlights(bookHash, highlights);

    HighlightSync::pushDelete(bookHash, id);
}

void setNote(const QString &bookHash, int index, const QString &note)
{
    QVector<Highlight> highlights = highlightsFor(bookHash);
    if (index < 0 || index >= highlights.size()) {
        return;
    }
    highlights[index].note = note;
    highlights[index].updatedAt = QDateTime::currentDateTime();
    writeHighlights(bookHash, highlights);

    HighlightSync::pushUpsert(bookHash, highlights[index]);
}

void setColor(const QString &bookHash, int index, const QColor &color)
{
    QVector<Highlight> highlights = highlightsFor(bookHash);
    if (index < 0 || index >= highlights.size()) {
        return;
    }
    highlights[index].color = color;
    highlights[index].updatedAt = QDateTime::currentDateTime();
    writeHighlights(bookHash, highlights);

    HighlightSync::pushUpsert(bookHash, highlights[index]);
}

void replaceMerged(const QString &bookHash, const QVector<Highlight> &merged)
{
    QVector<Highlight> sorted = merged;
    sortByTarget(sorted);
    writeHighlights(bookHash, sorted);
}

} // namespace HighlightStore
