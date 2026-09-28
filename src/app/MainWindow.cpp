#include "MainWindow.h"
#include "markdown/Parser.h"
#include "render/Renderer.h"
#include <QAction>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSaveFile>
#include <QSplitter>
#include <QTextBrowser>
#include <QTimer>
#include <QTreeView>
#include <QUrl>

MainWindow::MainWindow(const QString &rootDir, QWidget *parent)
    : QMainWindow(parent),
      files_(new QTreeView),
      model_(new QFileSystemModel(this)),
      editor_(new QPlainTextEdit),
      preview_(new QTextBrowser)
{

    rootDir_ = rootDir;
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

    editor_->setReadOnly(true);

    renderTimer_ = new QTimer(this);
    renderTimer_->setSingleShot(true);
    renderTimer_->setInterval(150);
    connect(renderTimer_, &QTimer::timeout, this, &MainWindow::refreshPreview);
    connect(editor_, &QPlainTextEdit::textChanged, renderTimer_, qOverload<>(&QTimer::start));

    connect(editor_->document(), &QTextDocument::modificationChanged, this, &MainWindow::updateTitle);

    preview_->setOpenLinks(false);
    connect(preview_, &QTextBrowser::anchorClicked, this, &MainWindow::onLinkClicked);

    auto *split = new QSplitter(Qt::Horizontal);
    split->addWidget(files_);
    split->addWidget(editor_);
    split->addWidget(preview_);
    split->setSizes({220, 540, 540});
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setStretchFactor(2, 1);
    setCentralWidget(split);

    auto *saveAct = new QAction(tr("&Save"), this);
    saveAct->setShortcut(QKeySequence::Save);
    connect(saveAct, &QAction::triggered, this, &MainWindow::save);
    menuBar()->addMenu(tr("&File"))->addAction(saveAct);

    updateTitle();
}

void MainWindow::updateTitle()
{
    if (currentPath_.isEmpty())
    {
        setWindowTitle("marky - " + rootDir_);
    }
    else
    {
        setWindowTitle(QFileInfo(currentPath_).fileName() + "[*] - marky");
    }
    setWindowModified(editor_->document()->isModified());
}

void MainWindow::loadFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "marky", "Cannot open " + path + ":\n" + f.errorString());
        return;
    }
    const QString text = QString::fromUtf8(f.readAll());

    currentPath_ = path;
    editor_->setReadOnly(false);
    editor_->setPlainText(text);
    editor_->document()->setModified(false); // freshly loaded = not modified

    renderTimer_->stop(); // no 150 ms wait when opening a file
    refreshPreview();
    updateTitle();
}

void MainWindow::onFileClicked(const QModelIndex &index)
{
    if (model_->isDir(index))
        return;

    const QString path = model_->filePath(index);
    if (path == currentPath_)
        return;

    if (!maybeSave())
    {
        files_->setCurrentIndex(model_->index(currentPath_)); // put the highlight back
        return;
    }
    loadFile(path);
}

bool MainWindow::save()
{
    if (currentPath_.isEmpty())
        return true;

    QSaveFile f(currentPath_); 
    if (!f.open(QIODevice::WriteOnly))
    {
        QMessageBox::warning(this, "marky", "Cannot save:\n" + f.errorString());
        return false;
    }
    f.write(editor_->toPlainText().toUtf8());
    if (!f.commit())
    {
        QMessageBox::warning(this, "marky", "Cannot save:\n" + f.errorString());
        return false;
    }
    editor_->document()->setModified(false);
    return true;
}

bool MainWindow::maybeSave()
{
    if (!editor_->document()->isModified())
        return true;

    const auto answer = QMessageBox::warning(
        this, "marky",
        QFileInfo(currentPath_).fileName() + " has unsaved changes.",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (answer == QMessageBox::Save)
        return save();
    return answer == QMessageBox::Discard;
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    if (maybeSave())
        e->accept();
    else
        e->ignore();
}

void MainWindow::refreshPreview()
{

    const QByteArray utf8 = editor_->toPlainText().toUtf8();
    auto root = md::parse(std::string_view(utf8.constData(), (size_t)utf8.size()));
    if (!root)
        return;

    const QString baseDir = currentPath_.isEmpty() ? rootDir_ : QFileInfo(currentPath_).absolutePath();
    const int maxW = qMax(200, preview_->viewport()->width() - 40);
    auto doc = render::toDocument(*root, baseDir, maxW);

    preview_->setDocument(doc.get());
    doc_ = std::move(doc);
}

void MainWindow::onLinkClicked(const QUrl &url)
{
    const QString scheme = url.scheme();
    if (scheme == "http" || scheme == "https" || scheme == "mailto")
        QDesktopServices::openUrl(url);
}