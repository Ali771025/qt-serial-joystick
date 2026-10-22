#include <QApplication>
#include <QFile>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app(argc, argv);
    app.setOrganizationName("Omrani");
    app.setApplicationName("JoystickMaster");
    app.setApplicationVersion("1.0.0");
    QFile style(":/style.qss");
    if (style.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(style.readAll()));
    MainWindow window;
    window.show();
    return app.exec();
}
