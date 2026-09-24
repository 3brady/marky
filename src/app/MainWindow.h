#pragma once
#include <QMainWindow>
#include <memory>
#include <QTextDocument>

class QPlainTextEdit;
class QTextBrowser;
class QTreeView;
class QFileSystemModel;
class QModelIndex;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(const QString &rootDir, QWidget *parent = nullptr);

private slots:
    void onFileClicked(const QModelIndex &index);
    void refreshPreview();

private:
    QTreeView *files_;
    QFileSystemModel *model_;
    QPlainTextEdit *editor_;
    QTextBrowser *preview_;
    std::unique_ptr<QTextDocument> doc_;
};