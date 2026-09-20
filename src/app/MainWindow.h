#pragma once
#include <QMainWindow>

class QPlainTextEdit;
class QTextBrowser;
class QTreeView;
class QFileSystemModel;
class QModelIndex;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString& rootDir, QWidget* parent = nullptr);

private slots:
    void onFileClicked(const QModelIndex& index);

private:
    QTreeView* files_;
    QFileSystemModel* model_;
    QPlainTextEdit* editor_;
    QTextBrowser* preview_;
};