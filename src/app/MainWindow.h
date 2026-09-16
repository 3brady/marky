#pragma once
#include <QMainWindow>

class QPlainTextEdit;
class QTextBrowser;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    QPlainTextEdit* editor_;
    QTextBrowser* preview_;
};
