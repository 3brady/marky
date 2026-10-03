#pragma once
#include <QMainWindow>
#include <memory>
#include<vector>
#include"render/Renderer.h"
#include <QTextDocument>

class QPlainTextEdit;
class QTextBrowser;
class QTreeView;
class QFileSystemModel;
class QModelIndex;
class QTimer;
class QCloseEvent;
class QUrl;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(const QString &rootDir, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *e) override;

private slots:
    void onFileClicked(const QModelIndex &index);
    void onLinkClicked(const QUrl &url);
    void refreshPreview();
    void updateTitle();
    void syncPreviewToEditor();
    void syncEditorToPreview();

private:
    bool save();      // false = failed
    bool maybeSave(); // false = user cancelled (or save failed)
    void loadFile(const QString &path);
    bool syncLock_ = false;      // true while we move a scrollbar ourselves
    double anchorY(const render::Anchor& a) const;

    QTreeView *files_;
    QFileSystemModel *model_;
    QPlainTextEdit *editor_;
    QTextBrowser *preview_;
    std::unique_ptr<QTextDocument> doc_;
    std::vector<render::Anchor> anchors_;

    QString rootDir_;
    QString currentPath_;
    QTimer *renderTimer_ = nullptr;
};