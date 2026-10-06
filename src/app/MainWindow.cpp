#include "MainWindow.h"
#include "markdown/Parser.h"
#include "render/Renderer.h"
#include "app/Theme.h"
#include <cstdio>
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
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QScrollBar>
#include <QSettings>
#include <algorithm>
#include <QFontDatabase>
#include <QPainter>
#include <QSplitterHandle>

namespace
{
    class LineHandle : public QSplitterHandle
    {
    public:
        using QSplitterHandle::QSplitterHandle;

    protected:
        void paintEvent(QPaintEvent *) override
        {
            QPainter p(this);
            p.fillRect(rect(), palette().color(QPalette::Window));
            p.setPen(palette().color(QPalette::Mid));
            const int x = width() / 2;
            p.drawLine(x, 0, x, height());
        }
    };

    class Splitter : public QSplitter
    {
    public:
        using QSplitter::QSplitter;

    protected:
        QSplitterHandle *createHandle() override { return new LineHandle(orientation(), this); }
    };

}

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

    auto *darkAct = new QAction(tr("&Dark mode"), this);
    darkAct->setCheckable(true);
    {
        QSettings settings;
        darkAct->setChecked(settings.value("dark", theme::systemPrefersDark()).toBool());
    }
    connect(darkAct, &QAction::toggled, this, [this](bool on)
            { theme::apply(on); QSettings().setValue("dark", on); refreshPreview(); });
    menuBar()->addMenu(tr("&View"))->addAction(darkAct);

    editor_->setReadOnly(true);

    renderTimer_ = new QTimer(this);
    renderTimer_->setSingleShot(true);
    renderTimer_->setInterval(150);
    connect(renderTimer_, &QTimer::timeout, this, &MainWindow::refreshPreview);
    connect(editor_, &QPlainTextEdit::textChanged, renderTimer_, qOverload<>(&QTimer::start));

    connect(editor_->document(), &QTextDocument::modificationChanged, this, &MainWindow::updateTitle);

    preview_->setOpenLinks(false);
    connect(preview_, &QTextBrowser::anchorClicked, this, &MainWindow::onLinkClicked);

    connect(editor_->verticalScrollBar(), &QScrollBar::valueChanged, this, &MainWindow::syncPreviewToEditor);
    connect(preview_->verticalScrollBar(), &QScrollBar::valueChanged, this, &MainWindow::syncEditorToPreview);

    auto* split = new Splitter(Qt::Horizontal);
    split->addWidget(files_);
    split->addWidget(editor_);
    split->addWidget(preview_);
    split->setSizes({220, 540, 540});
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setStretchFactor(2, 1);
    files_->setFrameShape(QFrame::NoFrame);
    editor_->setFrameShape(QFrame::NoFrame);
    preview_->setFrameShape(QFrame::NoFrame);
    split->setHandleWidth(6);
    files_->setBackgroundRole(QPalette::Window);
    files_->viewport()->setBackgroundRole(QPalette::Window);
    files_->setIndentation(16);
    editor_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    editor_->document()->setDocumentMargin(16);
    editor_->setTabStopDistance(4 * editor_->fontMetrics().horizontalAdvance(' '));
    setCentralWidget(split);
    editor_->document()->setModified(false);

    auto *saveAct = new QAction(tr("&Save"), this);
    saveAct->setShortcut(QKeySequence::Save);
    connect(saveAct, &QAction::triggered, this, &MainWindow::save);
    menuBar()->addMenu(tr("&File"))->addAction(saveAct);

    updateTitle();
}

void MainWindow::updateTitle()
{
    const bool hasFile = !currentPath_.isEmpty();
    setWindowTitle(hasFile ? QFileInfo(currentPath_).fileName() + "[*] - marky"
                           : "marky - " + rootDir_);
    setWindowModified(hasFile && editor_->document()->isModified());
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
    if (currentPath_.isEmpty() || !editor_->document()->isModified())
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

    auto r = render::toDocument(*root, baseDir, maxW);
    syncLock_ = true;
    preview_->setDocument(r.doc.get());
    doc_ = std::move(r.doc);
    anchors_ = std::move(r.anchors);

    QTimer::singleShot(0, this, [this]
                       { syncLock_ = false; syncPreviewToEditor(); });
}

double MainWindow::anchorY(const render::Anchor &a) const
{
    return doc_->documentLayout()->blockBoundingRect(doc_->findBlock(a.position)).top();
}

// editor -> preview
void MainWindow::syncPreviewToEditor()
{
    if (syncLock_ || !doc_ || anchors_.empty())
        return;
    syncLock_ = true;

    QScrollBar *from = editor_->verticalScrollBar();
    QScrollBar *to = preview_->verticalScrollBar();

    if (from->maximum() > 0 && from->value() >= from->maximum())
    {
        to->setValue(to->maximum());
    }
    else
    {
        const int line = editor_->cursorForPosition(QPoint(0, 0)).blockNumber();

        auto next = std::upper_bound(anchors_.begin(), anchors_.end(), line,
                                     [](int l, const render::Anchor &a)
                                     { return l < a.line; });

        double y = 0;
        if (next != anchors_.begin())
        {
            const auto &prev = *(next - 1);
            const double y0 = anchorY(prev), l0 = prev.line;
            double y1, l1;
            if (next != anchors_.end())
            {
                y1 = anchorY(*next);
                l1 = next->line;
            }
            else
            {
                y1 = doc_->size().height();
                l1 = editor_->document()->blockCount();
            }
            y = (l1 > l0) ? y0 + (line - l0) / (l1 - l0) * (y1 - y0) : y0;
        }
        to->setValue(int(y));
    }
    syncLock_ = false;
}

// preview -> editor
void MainWindow::syncEditorToPreview()
{
    if (syncLock_ || !doc_ || anchors_.empty())
        return;
    syncLock_ = true;

    QScrollBar *from = preview_->verticalScrollBar();
    QScrollBar *to = editor_->verticalScrollBar();

    if (from->maximum() > 0 && from->value() >= from->maximum())
    {
        to->setValue(to->maximum());
    }
    else
    {
        const double y = from->value();

        auto next = std::upper_bound(anchors_.begin(), anchors_.end(), y,
                                     [this](double yy, const render::Anchor &a)
                                     { return yy < anchorY(a); });

        double line = 0;
        if (next != anchors_.begin())
        {
            const auto &prev = *(next - 1);
            const double y0 = anchorY(prev), l0 = prev.line;
            double y1, l1;
            if (next != anchors_.end())
            {
                y1 = anchorY(*next);
                l1 = next->line;
            }
            else
            {
                y1 = doc_->size().height();
                l1 = editor_->document()->blockCount();
            }
            line = (y1 > y0) ? l0 + (y - y0) / (y1 - y0) * (l1 - l0) : l0;
        }

        const int b = qBound(0, int(line), editor_->document()->blockCount() - 1);
        const QTextBlock blk = editor_->document()->findBlockByNumber(b);
        const double frac = line - b;
        to->setValue(int(blk.firstLineNumber() + frac * qMax(1, blk.lineCount())));
    }
    syncLock_ = false;
}

void MainWindow::onLinkClicked(const QUrl &url)
{
    const QString scheme = url.scheme();
    if (scheme == "http" || scheme == "https" || scheme == "mailto")
        QDesktopServices::openUrl(url);
}