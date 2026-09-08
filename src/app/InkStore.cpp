#include "InkStore.h"

#include <QCryptographicHash>
#include <QSettings>
#include <QStringList>
#include <QUuid>

namespace {

QString groupKeyForInk(const QString &bookHash)
{
    const QByteArray hash = QCryptographicHash::hash(bookHash.toUtf8(), QCryptographicHash::Md5).toHex();
    return QStringLiteral("Ink/%1").arg(QString::fromLatin1(hash));
}

// QSettings' INI backend round-trips simple QVariant types reliably, but not
// something like QVariantList<QPointF> -- encoded as a plain "x1,y1;x2,y2;
// ..." string instead, sidestepping that entirely.
QString encodePoints(const QVector<QPointF> &points)
{
    QStringList parts;
    parts.reserve(points.size());
    for (const QPointF &p : points) {
        parts.append(QStringLiteral("%1,%2").arg(p.x()).arg(p.y()));
    }
    return parts.join(QLatin1Char(';'));
}

QVector<QPointF> decodePoints(const QString &encoded)
{
    QVector<QPointF> points;
    const QStringList parts = encoded.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    points.reserve(parts.size());
    for (const QString &part : parts) {
        const QStringList xy = part.split(QLatin1Char(','));
        if (xy.size() == 2) {
            points.append(QPointF(xy[0].toDouble(), xy[1].toDouble()));
        }
    }
    return points;
}

void writeStrokes(const QString &bookHash, const QVector<InkStroke> &strokes)
{
    QSettings settings;
    const QString group = groupKeyForInk(bookHash);
    settings.remove(group);
    settings.beginWriteArray(group);
    for (int i = 0; i < strokes.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("targetIndex", strokes[i].targetIndex);
        settings.setValue("points", encodePoints(strokes[i].points));
        settings.setValue("color", strokes[i].color.rgba());
        settings.setValue("width", strokes[i].width);
        settings.setValue("id", strokes[i].id);
    }
    settings.endArray();
}

} // namespace

namespace InkStore {

QVector<InkStroke> strokesFor(const QString &bookHash)
{
    QSettings settings;
    QVector<InkStroke> result;
    const int size = settings.beginReadArray(groupKeyForInk(bookHash));
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        InkStroke stroke;
        stroke.targetIndex = settings.value("targetIndex", -1).toInt();
        stroke.points = decodePoints(settings.value("points").toString());
        stroke.color = QColor::fromRgba(settings.value("color", QColor(Qt::black).rgba()).toUInt());
        stroke.width = settings.value("width", 2.0).toDouble();
        stroke.id = settings.value("id").toString();
        if (stroke.points.size() >= 2) {
            result.append(stroke);
        }
    }
    settings.endArray();
    return result;
}

void addStroke(const QString &bookHash, const InkStroke &strokeIn)
{
    InkStroke stroke = strokeIn;
    stroke.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QVector<InkStroke> strokes = strokesFor(bookHash);
    strokes.append(stroke);
    writeStrokes(bookHash, strokes);
}

void clearPage(const QString &bookHash, int pageIndex)
{
    QVector<InkStroke> strokes = strokesFor(bookHash);
    QVector<InkStroke> remaining;
    remaining.reserve(strokes.size());
    for (const InkStroke &stroke : strokes) {
        if (stroke.targetIndex != pageIndex) {
            remaining.append(stroke);
        }
    }
    if (remaining.size() != strokes.size()) {
        writeStrokes(bookHash, remaining);
    }
}

} // namespace InkStore
