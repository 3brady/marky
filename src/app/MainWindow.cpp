#include "MainWindow.h"
#include <QFile>
#include <QFileSystemModel>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QTextBrowser>
#include <QTreeView>

MainWindow::MainWindow(const QString& rootDir, QWidget* parent)
    : QMainWindow(parent),
      files_(new QTreeView),
      model_(new QFileSystemModel(this)),
      editor_(new QPlainTextEdit),
      preview_(new QTextBrowser) {

    model_->setReadOnly(true);
    model_->setNameFilters({"*.md", "*.markdown"});
    model_->setNameFilterDisables(false); 
    model_->setRootPath(rootDir);

    files_->setModel(model_);
    files_->setRootIndex(model_->index(rootDir));
    for (int c = 1; c < model_->columnCount(); ++c)
        files_->hideColumn(c);                   
    files_->setHeaderHidden(true);

    connect(files_, &QTreeView::clicked, this, &MainWindow::onFileClicked);

    auto* split = new QSplitter(Qt::Horizontal);
    split->addWidget(files_);
    split->addWidget(editor_);
    split->addWidget(preview_);
    split->setSizes({220, 540, 540});
    split->setStretchFactor(0, 0);   
    split->setStretchFactor(1, 1);
    split->setStretchFactor(2, 1);
    setCentralWidget(split);

    setWindowTitle("marky - " + rootDir);
}

void MainWindow::onFileClicked(const QModelIndex& index) {
    if (model_->isDir(index)) return;

    QFile f(model_->filePath(index));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    editor_->setPlainText(QString::fromUtf8(f.readAll()));
}