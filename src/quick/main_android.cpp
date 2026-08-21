#include "SmokeTestBridge.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    SmokeTestBridge smokeTestBridge;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("smokeTestBridge", &smokeTestBridge);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("MnemosyneAndroid", "Main");

    return app.exec();
}
