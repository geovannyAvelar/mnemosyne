#include "AndroidStorageAccess.h"
#include "LibraryModel.h"
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
    QQmlContext *context = engine.rootContext();
    context->setContextProperty("smokeTestBridge", &smokeTestBridge);
    context->setContextProperty("androidStorageAccess", &androidStorageAccess);
    context->setContextProperty("libraryModel", &libraryModel);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("MnemosyneAndroid", "Main");

    return app.exec();
}
