#include "EpubReaderModel.h"
#include "HighlightsModel.h"
#include "IOSStorageAccess.h"
#include "LibraryModel.h"
#include "PdfDocumentModel.h"
#include "PdfPageImageProvider.h"
#include "PdfSelectionController.h"
#include "PopplerFontSetup.h"
#include "SmokeTestBridge.h"
#include "SyncController.h"
#include "ThemeSettings.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    setupPopplerBase14Fonts();

    SmokeTestBridge smokeTestBridge;
    IOSStorageAccess iosStorageAccess;
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
    EpubReaderModel epubReaderModel;
    HighlightsModel highlightsModel;
    PdfSelectionController pdfSelectionController(&pdfDocumentModel);
    SyncController syncController;
    ThemeSettings themeSettings;

    QQmlContext *context = engine.rootContext();
    context->setContextProperty("smokeTestBridge", &smokeTestBridge);
    context->setContextProperty("documentPicker", &iosStorageAccess);
    context->setContextProperty("libraryModel", &libraryModel);
    context->setContextProperty("pdfDocumentModel", &pdfDocumentModel);
    context->setContextProperty("epubReaderModel", &epubReaderModel);
    context->setContextProperty("highlightsModel", &highlightsModel);
    context->setContextProperty("pdfSelectionController", &pdfSelectionController);
    context->setContextProperty("syncController", &syncController);
    context->setContextProperty("themeSettings", &themeSettings);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("MnemosyneIOS", "Main");

    return app.exec();
}
