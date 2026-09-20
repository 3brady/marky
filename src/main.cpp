#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <cstdio>
#include "app/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    const QStringList args = app.arguments();
    const QString dir = args.size() > 1 ? args.at(1) : QDir::currentPath();

    QFileInfo info(dir);
    if (!info.isDir()) {
        std::fprintf(stderr, "marky: '%s' is not a directory\n", qPrintable(dir));
        return 1;
    }

    MainWindow w(info.absoluteFilePath());
    w.resize(1300, 700);
    w.show();
    return app.exec();
}