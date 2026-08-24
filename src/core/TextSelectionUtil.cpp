#include "TextSelectionUtil.h"

#include <algorithm>
#include <limits>

int nearestWordIndex(const QVector<TextWord> &words, const QPointF &point)
{
    if (words.isEmpty()) {
        return -1;
    }

    int bestIndex = 0;
    qreal bestScore = std::numeric_limits<qreal>::max();
    for (int i = 0; i < words.size(); ++i) {
        const QRectF &box = words[i].boundingBox;

        qreal dx = 0;
        if (point.x() < box.left()) {
            dx = box.left() - point.x();
        } else if (point.x() > box.right()) {
            dx = point.x() - box.right();
        }

        qreal dy = 0;
        if (point.y() < box.top()) {
            dy = box.top() - point.y();
        } else if (point.y() > box.bottom()) {
            dy = point.y() - box.bottom();
        }

        const qreal score = dy * 1000.0 + dx;
        if (score < bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }
    return bestIndex;
}

TextSelectionResult selectWordRange(const QVector<TextWord> &words, const QPointF &anchorPoint,
                                     const QPointF &focusPoint)
{
    TextSelectionResult result;

    if (words.isEmpty()) {
        return result;
    }

    int anchorIndex = nearestWordIndex(words, anchorPoint);
    int focusIndex = nearestWordIndex(words, focusPoint);
    if (anchorIndex < 0 || focusIndex < 0) {
        return result;
    }
    if (anchorIndex > focusIndex) {
        std::swap(anchorIndex, focusIndex);
    }

    QString text;
    QVector<QRectF> wordRects;
    for (int i = anchorIndex; i <= focusIndex; ++i) {
        const TextWord &word = words[i];
        wordRects.append(word.boundingBox);
        text += word.text;

        if (i < focusIndex) {
            const TextWord &next = words[i + 1];
            const qreal verticalGap = std::abs(next.boundingBox.center().y() - word.boundingBox.center().y());
            if (verticalGap > word.boundingBox.height() / 2.0) {
                text += QLatin1Char('\n');
            } else if (word.hasSpaceAfter) {
                text += QLatin1Char(' ');
            }
        }
    }

    result.text = text;
    result.wordRects = wordRects;
    return result;
}
