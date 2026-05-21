#include <QApplication>
#include <QSurfaceFormat>

#include "MainWindow.h"
#include "audio/AudioRouting.h"
#include "util/CrashHandler.h"

int main(int argc, char* argv[]) {
    // Install the crash handler very early — before QApplication, before
    // anything that might segfault. Hooks registered here will run on any
    // fatal signal (SIGSEGV / SIGABRT / SIGFPE / SIGILL / SIGBUS), then the
    // signal is re-raised with the default handler so a coredump can still
    // be produced.
    CrashHandler::install();
    CrashHandler::addCrashHook(&AudioRouting::signalSafeRestore);

    // Set OpenGL 4.3 Core profile before QApplication is constructed.
    // Must happen first — see Qt docs on QSurfaceFormat::setDefaultFormat.
    QSurfaceFormat fmt;
    fmt.setVersion(4, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setSamples(4);    // MSAA x4
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    app.setApplicationName("Scintilla");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("Darian-Frey");

    // On clean exit, restore the system default audio source via QProcess
    // (the crash-path uses the signal-safe variant registered above).
    QObject::connect(&app, &QApplication::aboutToQuit,
                     []() { AudioRouting::restoreSaved(); });

    MainWindow window;
    window.show();

    return app.exec();
}
