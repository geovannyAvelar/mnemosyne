#include "HighlightStore.h"

#include <QCryptographicHash>
#include <QSettings>

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
    }
    settings.endArray();
}

} // namespace

namespace HighlightStore {

QVector<Highlight> highlightsFor(const QString &bookHash)
{
    QSettings settings;
    QVector<Highlight> result;
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
        result.append(h);
    }
    settings.endArray();

    std::sort(result.begin(), result.end(), [](const Highlight &a, const Highlight &b) {
        return a.targetIndex < b.targetIndex;
    });
    return result;
}

void addHighlight(const QString &bookHash, const Highlight &highlight)
{
    QVector<Highlight> highlights = highlightsFor(bookHash);
    highlights.append(highlight);
    std::sort(highlights.begin(), highlights.end(), [](const Highlight &a, const Highlight &b) {
        return a.targetIndex < b.targetIndex;
    });
    writeHighlights(bookHash, highlights);
}

void removeHighlight(const QString &bookHash, int index)
{
    QVector<Highlight> highlights = highlightsFor(bookHash);
    if (index < 0 || index >= highlights.size()) {
        return;
    }
    highlights.removeAt(index);
    writeHighlights(bookHash, highlights);
}

} // namespace HighlightStore
