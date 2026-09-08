#include "EmbeddedImageHtml.h"

#include "core/HtmlAttrUtil.h"

#include <QImage>
#include <QRegularExpression>

namespace EmbeddedImageHtml {

QString mobiImageReference(const QString &imgTag)
{
    const QString recindex = extractHtmlAttr(imgTag, QStringLiteral("recindex"));
    if (!recindex.isEmpty()) {
        return recindex;
    }
    static const QRegularExpression kindleEmbedRe(QStringLiteral("kindle:embed:0*(\\d+)"),
                                                    QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = kindleEmbedRe.match(extractHtmlAttr(imgTag, QStringLiteral("src")));
    return m.hasMatch() ? m.captured(1) : QString();
}

QString rewriteImgTagWithDataUri(QString imgTag, const QByteArray &imageData, const QString &mimeType, int maxWidth)
{
    static const QRegularExpression srcRe(QStringLiteral("\\bsrc\\s*=\\s*(\"[^\"]*\"|'[^']*')"),
                                           QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression recindexRe(QStringLiteral("\\s*\\brecindex\\s*=\\s*(\"[^\"]*\"|'[^']*')"),
                                                QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression widthAttrRe(QStringLiteral("\\s*\\bwidth\\s*=\\s*(\"[^\"]*\"|'[^']*')"),
                                                 QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression heightAttrRe(QStringLiteral("\\s*\\bheight\\s*=\\s*(\"[^\"]*\"|'[^']*')"),
                                                  QRegularExpression::CaseInsensitiveOption);

    const QString dataUri =
        QStringLiteral("data:%1;base64,%2").arg(mimeType, QString::fromLatin1(imageData.toBase64()));

    if (srcRe.match(imgTag).hasMatch()) {
        imgTag.replace(srcRe, QStringLiteral("src=\"%1\"").arg(dataUri));
    } else {
        // Position 4: right after "<img" (or "<IMG" -- callers match case-
        // insensitively, but either spelling is 4 characters).
        imgTag.insert(4, QStringLiteral(" src=\"%1\"").arg(dataUri));
    }
    imgTag.remove(recindexRe);

    const QImage image = QImage::fromData(imageData);
    if (!image.isNull() && image.width() > maxWidth) {
        const int scaledHeight = (image.height() * maxWidth) / image.width();
        imgTag.remove(widthAttrRe);
        imgTag.remove(heightAttrRe);
        imgTag.insert(4, QStringLiteral(" width=\"%1\" height=\"%2\"").arg(maxWidth).arg(scaledHeight));
    }

    return imgTag;
}

} // namespace EmbeddedImageHtml
