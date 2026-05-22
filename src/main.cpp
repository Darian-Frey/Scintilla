#include <QApplication>
#include <QSurfaceFormat>

#include "MainWindow.h"
#include "util/CrashHandler.h"

int main(int argc, char* argv[]) {
    // Install the crash handler very early — before QApplication, before
    // anything that might segfault. Hooks registered here will run on any
    // fatal signal (SIGSEGV / SIGABRT / SIGFPE / SIGILL / SIGBUS), then the
    // signal is re-raised with the default handler so a coredump can still
    // be produced. No audio-restore hook is needed any more: monitor
    // routing is now per-stream via pactl move-source-output, so there's
    // no global state to undo on exit (BUG-013 / IMP-009).
    CrashHandler::install();

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

    MainWindow window;
    window.show();

    return app.exec();
}
