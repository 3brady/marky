#include "MainWindow.h"
#include "markdown/Parser.h"
#include "render/Renderer.h"
#include "style/Theme.h"
#include "style/Highlighter.h"
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
#include <QMenu>
#include <QWheelEvent>
#include <QDir>
#include <QInputDialog>
#include <QStatusBar>
#include <QFileDialog>
#include <QPdfWriter>
#include <QPageSize>
#include <QTextDocument>
#include <QHash>
#include <QRegularExpression>
#include <QTextCursor>
#include <QFileSystemWatcher>

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

    static QString cleanName(QString name)
    {
        name = name.trimmed();
        if (name.isEmpty() || name.contains('/') || name.contains('\\') || name.startsWith('.'))
            return {};
        if (!name.endsWith(".md", Qt::CaseInsensitive) && !name.endsWith(".markdown", Qt::CaseInsensitive))
            name += ".md";
        return name;
    }

    QString slugify(QString s)
    {
        // i really hate regex
        static const QRegularExpression link(R"(!?\[([^\]]*)\]\([^)]*\))");
        static const QRegularExpression marks(R"([`*~])");
        s.replace(link, "\\1");
        s.remove(marks);
        s = s.trimmed().toLower();

        QString out;
        for (QChar c : s)
        {
            if (c.isLetterOrNumber() || c == '_' || c == '-')
                out += c;
            else if (c == ' ')
                out += '-';
        }
        return out;
    }

}

MainWindow::MainWindow(const QString &rootDir, QWidget *parent)
    : QMainWindow(parent),
      files_(new QTreeView),
      model_(new QFileSystemModel(this)),
      editor_(new QPlainTextEdit),
      highlighter_(new Highlighter(editor_->document())),
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
            { theme::apply(on); highlighter_->refresh(on); QSettings().setValue("dark", on); refreshPreview(); });

    auto *zoomIn = new QAction(tr("Zoom &In"), this);
    zoomIn->setShortcuts({QKeySequence(QKeySequence::ZoomIn), QKeySequence("Ctrl+=")}); // Ctrl+= needs no Shift
    connect(zoomIn, &QAction::triggered, this, [this]
            { setFontSize(fontSize_ + 1); });

    auto *zoomOut = new QAction(tr("Zoom &Out"), this);
    zoomOut->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOut, &QAction::triggered, this, [this]
            { setFontSize(fontSize_ - 1); });

    auto *zoomReset = new QAction(tr("&Reset Zoom"), this);
    zoomReset->setShortcut(QKeySequence("Ctrl+0"));
    connect(zoomReset, &QAction::triggered, this, [this]
            { setFontSize(defaultFontSize_); });

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(darkAct);
    viewMenu->addSeparator();
    viewMenu->addActions({zoomIn, zoomOut, zoomReset});

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

    watcher_ = new QFileSystemWatcher(this);
    connect(watcher_, &QFileSystemWatcher::fileChanged, this, [this](const QString &)
            { onExternalChange(); });

    auto *split = new Splitter(Qt::Horizontal);
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
    editor_->document()->setDocumentMargin(16);
    setCentralWidget(split);
    editor_->document()->setModified(false);
    defaultFontSize_ = QGuiApplication::font().pointSizeF() > 0 ? QGuiApplication::font().pointSizeF() : 11;
    fontSize_ = qBound(7.0, QSettings().value("fontSize", defaultFontSize_).toDouble(), 40.0);
    applyFontSize();

    editor_->viewport()->installEventFilter(this); // for Ctrl+wheel
    preview_->viewport()->installEventFilter(this);

    auto *newAct = new QAction(tr("&New File..."), this);
    newAct->setShortcut(QKeySequence::New);
    connect(newAct, &QAction::triggered, this, &MainWindow::newFile);

    auto *saveAct = new QAction(tr("&Save"), this);
    saveAct->setShortcut(QKeySequence::Save);
    connect(saveAct, &QAction::triggered, this, &MainWindow::save);

    auto *saveAsAct = new QAction(tr("Save &As..."), this);
    saveAsAct->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::saveAs);

    auto *quitAct = new QAction(tr("&Quit"), this);
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, this, &QWidget::close);

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(newAct);
    fileMenu->addAction(saveAct);
    fileMenu->addAction(saveAsAct);
    fileMenu->addSeparator();
    fileMenu->addAction(quitAct);
    fileMenu->addSeparator();

    auto *exportPdfAct = fileMenu->addAction(tr("Export as &PDF"));
    connect(exportPdfAct, &QAction::triggered, this, &MainWindow::exportPdf);

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
    watchCurrent();
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
    watchCurrent();
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
    render::Options opt;
    opt.baseDir = baseDir;
    opt.maxImageWidth = qMax(200, preview_->viewport()->width() - 40);
    opt.fontPointSize = fontSize_;
    auto r = render::toDocument(*root, opt);

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

void MainWindow::gotoHeading(const QString &fragment)
{
    static const QRegularExpression headingRe(R"(^\s{0,3}#{1,6}\s+(.*?)(?:\s+#+)?\s*$)");
    static const QRegularExpression fenceRe(R"(^\s{0,3}(```|~~~))");

    const QString wanted = QUrl::fromPercentEncoding(fragment.toUtf8()).toLower();
    QHash<QString, int> seen;
    bool inCode = false;

    for (QTextBlock b = editor_->document()->begin(); b.isValid(); b = b.next())
    {
        const QString line = b.text();
        if (fenceRe.match(line).hasMatch())
        {
            inCode = !inCode;
            continue;
        }
        if (inCode)
            continue;

        const auto m = headingRe.match(line);
        if (!m.hasMatch())
            continue;

        QString slug = slugify(m.captured(1));
        const int n = seen[slug]++;
        if (n > 0)
            slug += "-" + QString::number(n);

        if (slug == wanted)
        {
            editor_->setTextCursor(QTextCursor(b));
            editor_->centerCursor();
            return;
        }
    }
    statusBar()->showMessage(tr("Section not found: %1").arg(fragment), 4000);
}

void MainWindow::onLinkClicked(const QUrl &url)
{
    const QString scheme = url.scheme();
    if (scheme == "http" || scheme == "https" || scheme == "mailto")
    {
        QDesktopServices::openUrl(url);
        return;
    }

    QString path;
    if (url.isRelative())
    {
        const QString rel = url.path(QUrl::FullyDecoded);
        if (rel.isEmpty())
        {
            if (!url.fragment().isEmpty())
                gotoHeading(url.fragment());
            return;
        }
        const QString baseDir = currentPath_.isEmpty()
                                    ? rootDir_
                                    : QFileInfo(currentPath_).absolutePath();
        path = QDir(baseDir).absoluteFilePath(rel);
    }
    else if (url.isLocalFile())
    {
        path = url.toLocalFile();
    }
    else
    {
        return;
    }

    const QFileInfo fi(path);
    if (!fi.isFile())
    {
        statusBar()->showMessage(tr("File not found: %1").arg(path), 4000);
        return;
    }

    const QString canon = fi.canonicalFilePath();
    const QString root = QDir(rootDir_).canonicalPath();
    const QString ext = fi.suffix().toLower();
    if (!canon.startsWith(root + '/') || (ext != "md" && ext != "markdown"))
    {
        statusBar()->showMessage(tr("Not a Markdown file in this project"), 4000);
        return;
    }

    if (canon != QFileInfo(currentPath_).canonicalFilePath())
    {
        if (!maybeSave())
            return;
        loadFile(canon);
        files_->setCurrentIndex(model_->index(canon));
    }

    if (!url.fragment().isEmpty())
        gotoHeading(url.fragment());
}

void MainWindow::applyFontSize()
{
    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setPointSizeF(fontSize_);

    const bool wasModified = editor_->document()->isModified();
    editor_->setFont(f);
    editor_->setTabStopDistance(4 * editor_->fontMetrics().horizontalAdvance(' '));
    editor_->document()->setModified(wasModified);
}

void MainWindow::setFontSize(double pt)
{
    pt = qBound(7.0, pt, 40.0);
    if (qFuzzyCompare(pt, fontSize_))
        return;
    fontSize_ = pt;
    QSettings().setValue("fontSize", fontSize_);
    applyFontSize();
    refreshPreview();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *e)
{
    if (e->type() == QEvent::Wheel &&
        (obj == editor_->viewport() || obj == preview_->viewport()))
    {
        auto *we = static_cast<QWheelEvent *>(e);
        if (we->modifiers() & Qt::ControlModifier)
        {
            const int dy = we->angleDelta().y();
            if (dy != 0)
                setFontSize(fontSize_ + (dy > 0 ? 1 : -1));
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, e);
}

QString MainWindow::targetDir() const
{
    const QModelIndex idx = files_->currentIndex();
    if (!idx.isValid())
        return rootDir_;
    const QString p = model_->filePath(idx);
    return model_->isDir(idx) ? p : QFileInfo(p).absolutePath();
}

void MainWindow::newFile()
{
    if (!maybeSave())
        return;

    bool ok = false;
    const QString input = QInputDialog::getText(this, tr("New file"), tr("File name:"),
                                                QLineEdit::Normal, "untitled.md", &ok);
    if (!ok)
        return;

    const QString name = cleanName(input);
    if (name.isEmpty())
    {
        QMessageBox::warning(this, "marky", tr("Please use a plain file name (no folders or slashes)."));
        return;
    }

    const QString path = QDir(targetDir()).absoluteFilePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::NewOnly))
    {
        QMessageBox::warning(this, "marky", tr("Cannot create %1:\n%2").arg(name, f.errorString()));
        return;
    }
    f.close();

    loadFile(path);
    files_->setCurrentIndex(model_->index(path));
}

bool MainWindow::saveAs()
{
    if (currentPath_.isEmpty())
        return true;

    bool ok = false;
    const QString input = QInputDialog::getText(this, tr("Save as"), tr("New file name:"),
                                                QLineEdit::Normal, QFileInfo(currentPath_).fileName(), &ok);
    if (!ok)
        return false;

    const QString name = cleanName(input);
    if (name.isEmpty())
    {
        QMessageBox::warning(this, "marky", tr("Please use a plain file name (no folders or slashes)."));
        return false;
    }

    const QString path = QDir(QFileInfo(currentPath_).absolutePath()).absoluteFilePath(name);
    if (path == currentPath_)
        return save();

    if (QFileInfo::exists(path) &&
        QMessageBox::question(this, "marky", tr("%1 the file already exists. Do you want to replace it?").arg(name)) != QMessageBox::Yes)
        return false;

    const QString old = currentPath_;
    currentPath_ = path;
    if (!save())
    {
        currentPath_ = old;
        return false;
    }

    updateTitle();
    files_->setCurrentIndex(model_->index(path));
    watchCurrent();
    return true;
}

void MainWindow::exportPdf()
{
    const QFileInfo cur(currentPath_);
    const QString suggested = currentPath_.isEmpty()
                                  ? rootDir_ + "/untitled.pdf"
                                  : cur.dir().filePath(cur.completeBaseName() + ".pdf");

    QString path = QFileDialog::getSaveFileName(this, tr("Export as PDF"), suggested,
                                                tr("PDF files (*.pdf)"));
    if (path.isEmpty())
    {
        return;
    }

    if (!path.endsWith(".pdf", Qt::CaseInsensitive))
    {
        path += ".pdf";
    }

    const QByteArray utf8 = editor_->toPlainText().toUtf8();
    auto root = md::parse(std::string_view(utf8.constData(), (size_t)utf8.size()));
    if (!root)
    {
        return;
    }

    render::Options opt;
    opt.baseDir = currentPath_.isEmpty() ? rootDir_ : cur.absolutePath();
    opt.maxImageWidth = 640;
    opt.fontPointSize = fontSize_;

    // stops the dark theme for a second so the pdf would be white and then turns it back on
    const bool dark = qApp->palette().color(QPalette::Base).lightness() < 128;

    if (dark)
    {
        theme::apply(false);
    }

    auto result = render::toDocument(*root, opt);

    if (dark)
    {
        theme::apply(true);
    }

    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(18, 18, 18, 18), QPageLayout::Millimeter);
    writer.setResolution(96);
    writer.setTitle(cur.completeBaseName());
    result.doc->print(&writer);

    statusBar()->showMessage(tr("Exported %1").arg(path), 4000);
}

void MainWindow::watchCurrent()
{
    if (!watcher_->files().isEmpty())
    {
        watcher_->removePaths(watcher_->files());
    }
    if (!currentPath_.isEmpty() && QFileInfo::exists(currentPath_))
    {
        watcher_->addPath(currentPath_);
    }
}

void MainWindow::onExternalChange()
{
    QTimer::singleShot(150, this, [this]
                       {
        watchCurrent();
        if (currentPath_.isEmpty() || askingReload_)
            return;

        QFile f(currentPath_);
        if (!f.open(QIODevice::ReadOnly))
            return;
        QByteArray disk = f.readAll();
        f.close();
        disk.replace("\r\n", "\n");

        if (disk == editor_->toPlainText().toUtf8())
            return;

        if (editor_->document()->isModified())
        {
            askingReload_ = true;
            const auto answer = QMessageBox::question(
                this, tr("File changed on disk"),
                tr("\"%1\" was changed by another program, and you have unsaved edits.\n\n"
                   "Reload it and lose your edits?")
                    .arg(QFileInfo(currentPath_).fileName()),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            askingReload_ = false;
            if (answer != QMessageBox::Yes)
                return;
        }

        const int pos = editor_->textCursor().position();
        const int scroll = editor_->verticalScrollBar()->value();

        loadFile(currentPath_);

        QTextCursor c(editor_->document());
        c.setPosition(qMin(pos, editor_->document()->characterCount() - 1));
        editor_->setTextCursor(c);
        editor_->verticalScrollBar()->setValue(scroll);

        statusBar()->showMessage(tr("Reloaded: changed on disk"), 4000); });
}