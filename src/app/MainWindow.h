#pragma once
#include <QMainWindow>
#include <memory>
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

private:
    bool save();      // false = failed
    bool maybeSave(); // false = user cancelled (or save failed)
    void loadFile(const QString &path);

    QTreeView *files_;
    QFileSystemModel *model_;
    QPlainTextEdit *editor_;
    QTextBrowser *preview_;
    std::unique_ptr<QTextDocument> doc_;

    QString rootDir_;
    QString currentPath_;
    QTimer *renderTimer_ = nullptr;
};