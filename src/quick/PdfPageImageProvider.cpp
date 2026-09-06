#include "PdfPageImageProvider.h"

#include "core/Document.h"

#include <QMutexLocker>
#include <QQuickImageResponse>
#include <QQuickTextureFactory>
#include <QRunnable>
#include <QThreadPool>

// Not in an anonymous namespace: PdfPageImageProvider's `friend class
// PdfPageImageResponse;` declaration names this exact (global-scope) class,
// so it needs to actually be that class, not an unrelated same-named one
// inside an anonymous namespace.
class PdfPageImageResponse : public QQuickImageResponse, public QRunnable
{
public:
    PdfPageImageResponse(PdfPageImageProvider *provider, int pageIndex, qreal scale, bool invertColors)
        : m_provider(provider)
        , m_pageIndex(pageIndex)
        , m_scale(scale)
        , m_invertColors(invertColors)
    {
        // QQuickImageResponse's contract is that the response deletes
        // itself once finished() has been emitted and consumed; disable
        // QThreadPool's own auto-delete (it would otherwise delete this
        // QRunnable right after run() returns, racing the QML engine still
        // reading textureFactory()/errorString() off the finished signal).
        setAutoDelete(false);
        connect(this, &QQuickImageResponse::finished, this, &QObject::deleteLater);
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    QString errorString() const override { return m_errorString; }

    void run() override
    {
        {
            QMutexLocker locker(&m_provider->m_mutex);
            if (m_provider->m_document) {
                std::unique_ptr<IPage> page = m_provider->m_document->page(m_pageIndex);
                if (page) {
                    m_image = page->renderToImage(m_scale);
                }
            }
        }
        if (m_image.isNull()) {
            m_errorString = QStringLiteral("Page %1 not available").arg(m_pageIndex);
        } else if (m_invertColors) {
            // Full RGB invert (alpha untouched) -- same "mirror the page"
            // negative Acrobat's dark mode applies, not a hue-preserving
            // "smart invert": simplest to implement, and matches desktop's
            // CompositionMode_Difference-against-white in PdfPageStackView.
            m_image.invertPixels(QImage::InvertRgb);
        }
        emit finished();
    }

private:
    PdfPageImageProvider *m_provider;
    int m_pageIndex;
    qreal m_scale;
    bool m_invertColors;
    QImage m_image;
    QString m_errorString;
};

void PdfPageImageProvider::setDocument(IDocument *document)
{
    QMutexLocker locker(&m_mutex);
    m_document = document;
}

QQuickImageResponse *PdfPageImageProvider::requestImageResponse(const QString &id, const QSize & /*requestedSize*/)
{
    QString coreId = id;
    const bool invertColors = coreId.endsWith(QLatin1String("-dark"));
    if (invertColors) {
        coreId.chop(5); // "-dark"
    }

    const int separatorIndex = coreId.lastIndexOf(QLatin1Char('-'));
    const int pageIndex = separatorIndex >= 0 ? coreId.left(separatorIndex).toInt() : coreId.toInt();
    const qreal scale = separatorIndex >= 0 ? coreId.mid(separatorIndex + 1).toDouble() : 1.0;

    auto *response = new PdfPageImageResponse(this, pageIndex, scale, invertColors);
    QThreadPool::globalInstance()->start(response);
    return response;
}
