#include "Document.h"

#include "pdf/PopplerPdfDocument.h"

#include <QFileInfo>

std::unique_ptr<IDocument> openDocument(const QString &filePath, QString *errorMessage)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();

    if (suffix == QLatin1String("pdf")) {
        return PopplerPdfDocument::load(filePath, errorMessage);
    }

    if (errorMessage) {
        *errorMessage = QObject::tr("Unsupported file type: .%1").arg(suffix);
    }
    return nullptr;
}
