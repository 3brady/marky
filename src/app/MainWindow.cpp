#include "MainWindow.h"
#include <QPlainTextEdit>
#include <QSplitter>
#include <QTextBrowser>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      editor_(new QPlainTextEdit),
      preview_(new QTextBrowser) {
    auto* split = new QSplitter(Qt::Horizontal);
    split->addWidget(editor_);
    split->addWidget(preview_);
    split->setSizes({550, 550});
    setCentralWidget(split);
    setWindowTitle("mdedit");
}
