#include "AndroidStorageAccess.h"
#include "LibraryModel.h"
#include "PdfDocumentModel.h"
#include "PdfPageImageProvider.h"
#include "SmokeTestBridge.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    SmokeTestBridge smokeTestBridge;
    AndroidStorageAccess androidStorageAccess;
    LibraryModel libraryModel;

    QQmlApplicationEngine engine;

    // Owned by the engine from here on (addImageProvider() takes
    // ownership); pdfPageImageProvider stays a valid raw pointer for
    // PdfDocumentModel to call setDocument() on for as long as engine is
    // alive, which outlives pdfDocumentModel below (reverse destruction
    // order of locals declared in this scope).
    auto *pdfPageImageProvider = new PdfPageImageProvider;
    engine.addImageProvider(QStringLiteral("pdfpage"), pdfPageImageProvider);

    PdfDocumentModel pdfDocumentModel(pdfPageImageProvider);

    QQmlContext *context = engine.rootContext();
    context->setContextProperty("smokeTestBridge", &smokeTestBridge);
    context->setContextProperty("androidStorageAccess", &androidStorageAccess);
    context->setContextProperty("libraryModel", &libraryModel);
    context->setContextProperty("pdfDocumentModel", &pdfDocumentModel);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("MnemosyneAndroid", "Main");

    return app.exec();
}
