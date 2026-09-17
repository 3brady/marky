#include <QApplication>
#include"app/MainWindow.h"
#include "markdown/Parser.h"

int main(int argc, char* argv[]) {
    md::dumpBlocks("# Hello\n\nsome text\n\n- a\n- b\n");
    QApplication app(argc, argv);
    MainWindow w;
    w.resize(1100, 700);
    w.show();
    return app.exec();
}
