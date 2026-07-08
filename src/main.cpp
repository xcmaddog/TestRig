#include "mainwindow.h"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("TestRig");
    app.setOrganizationName("CREATE Lab");

    // Fusion style scales reasonably on high-DPI screens and on the Pi's
    // display.  Remove this line to use the platform default.
    app.setStyle(QStyleFactory::create("Fusion"));

    MainWindow w;
    w.show();
    return app.exec();
}
