#include "core/PianoController.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("SmartPianoQt"));
    QGuiApplication::setOrganizationName(QStringLiteral("SmartPiano"));
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    PianoController controller;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("piano"), &controller);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);

    engine.loadFromModule(QStringLiteral("SmartPianoQt"), QStringLiteral("Main"));
    return app.exec();
}
