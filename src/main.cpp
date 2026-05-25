#include "core/PianoController.h"
#include "midi/MidiInputService.h"
#include "ui/PianoRollItem.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <qqml.h>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("SmartPianoQt"));
    QGuiApplication::setOrganizationName(QStringLiteral("SmartPiano"));
    QQuickStyle::setStyle(QStringLiteral("Fusion"));
    qmlRegisterType<PianoRollItem>("SmartPianoQt.Controls", 1, 0, "PianoRollView");

    PianoController controller;
    MidiInputService midiInput;
    QObject::connect(&midiInput, &MidiInputService::noteOn,
                     &controller, &PianoController::noteOn);
    QObject::connect(&midiInput, &MidiInputService::noteOff,
                     &controller, &PianoController::noteOff);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("piano"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("midiInput"), &midiInput);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);

    engine.loadFromModule(QStringLiteral("SmartPianoQt"), QStringLiteral("Main"));
    return app.exec();
}
