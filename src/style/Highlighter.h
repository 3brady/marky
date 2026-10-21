#pragma once
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class Highlighter : public QSyntaxHighlighter {
public:
    explicit Highlighter(QTextDocument* doc);

    void refresh(bool dark);

protected:
    void highlightBlock(const QString& text) override;

private:
    void buildFormats(bool dark);
    void paint(int start, int len, const QTextCharFormat& f);

    QTextCharFormat heading_, marker_, quote_, code_, fence_, link_, strong_, em_, del_, rule_;
};