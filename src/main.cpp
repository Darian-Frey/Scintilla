#include <QApplication>
#include <QSurfaceFormat>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
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
