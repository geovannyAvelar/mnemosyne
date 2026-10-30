#include "Document.h"

#include "comic/CbzDocument.h"
#include "pdf/PopplerPdfDocument.h"

#include <QFileInfo>

std::unique_ptr<IDocument> openDocument(const QString &filePath, QString *errorMessage, const QString &password)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();

    if (suffix == QLatin1String("pdf")) {
        return PopplerPdfDocument::load(filePath, errorMessage, password);
    }

    if (suffix == QLatin1String("cbz")) {
        return CbzDocument::load(filePath, errorMessage);
    }

    if (errorMessage) {
        *errorMessage = QObject::tr("Unsupported file type: .%1").arg(suffix);
    }
    return nullptr;
}
