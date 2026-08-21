#include "TxtDocument.h"

#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QStringConverter>
#include <QTextStream>

std::unique_ptr<TxtDocument> TxtDocument::load(const QString &filePath, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Could not open file: %1").arg(filePath);
        }
        return nullptr;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    auto document = std::unique_ptr<TxtDocument>(new TxtDocument());
    document->m_text = stream.readAll();
    document->m_title = QFileInfo(filePath).completeBaseName();
    return document;
}
