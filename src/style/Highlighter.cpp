#include "style/Highlighter.h"

#include <QApplication>
#include <QPalette>
#include <QRegularExpression>

Highlighter::Highlighter(QTextDocument *doc) : QSyntaxHighlighter(doc)
{
    buildFormats(qApp->palette().color(QPalette::Base).lightness() < 128);
}

void Highlighter::refresh(bool dark)
{
    buildFormats(dark);
    rehighlight();
}

void Highlighter::buildFormats(bool dark)
{

    auto col = [&](const char *light, const char *darkc)
    { return QColor(dark ? darkc : light); };

    heading_ = QTextCharFormat();
    heading_.setForeground(col("#1a5fb4", "#78aeed"));
    heading_.setFontWeight(QFont::Bold);

    marker_ = QTextCharFormat();
    marker_.setForeground(col("#b35900", "#b7a00a"));

    quote_ = QTextCharFormat();
    quote_.setForeground(col("#5e5c64", "#9a9996"));

    code_ = QTextCharFormat();
    code_.setForeground(col("#a51d2d", "#f66151"));
    code_.setBackground(col("#f0f0f0", "#3a3a3a"));

    fence_ = QTextCharFormat();
    fence_.setForeground(col("#8a8a8a", "#8a8a8a"));
    fence_.setBackground(col("#f0f0f0", "#3a3a3a"));

    link_ = QTextCharFormat();
    link_.setForeground(col("#1c71d8", "#62a0ea"));
    link_.setFontUnderline(true);

    strong_ = QTextCharFormat();
    strong_.setFontWeight(QFont::Bold);

    em_ = QTextCharFormat();
    em_.setFontItalic(true);

    del_ = QTextCharFormat();
    del_.setFontStrikeOut(true);
    del_.setForeground(col("#8a8a8a", "#8a8a8a"));

    rule_ = QTextCharFormat();
    rule_.setForeground(col("#8a8a8a", "#8a8a8a"));
}

void Highlighter::paint(int start, int len, const QTextCharFormat &f)
{
    QTextCharFormat merged = format(start);
    merged.merge(f);
    setFormat(start, len, merged);
}

void Highlighter::highlightBlock(const QString &text)
{
    static const QRegularExpression fenceRe(R"(^\s{0,3}(`{3,}|~{3,}))");
    static const QRegularExpression ruleRe(R"(^\s{0,3}([-*_])(?:\s*\1){2,}\s*$)");
    static const QRegularExpression headingRe(R"(^\s{0,3}(#{1,6})(?:\s|$))");
    static const QRegularExpression quoteRe(R"(^\s{0,3}(?:>\s?)+)");
    static const QRegularExpression listRe(R"(^\s*(?:[-*+]|\d{1,9}[.)])\s+(?:\[[ xX]\]\s)?)");

    setCurrentBlockState(0);
    const bool inCode = previousBlockState() == 1;
    const auto fence = fenceRe.match(text);

    if (inCode)
    {
        const bool closes = fence.hasMatch() && text.trimmed() == fence.captured(1);
        if (closes)
        {
            setFormat(0, text.size(), fence_);
        }
        else
        {
            setCurrentBlockState(1);
            setFormat(0, text.size(), code_);
        }
        return;
    }
    if (fence.hasMatch())
    {
        setCurrentBlockState(1);
        setFormat(0, text.size(), fence_);
        return;
    }

    if (ruleRe.match(text).hasMatch())
    {
        setFormat(0, text.size(), rule_);
        return;
    }

    if (const auto m = headingRe.match(text); m.hasMatch())
    {
        paint(0, text.size(), heading_);
        setFormat(m.capturedStart(1), m.capturedLength(1), marker_);
    }
    else if (const auto q = quoteRe.match(text); q.hasMatch())
    {
        paint(0, text.size(), quote_);
        setFormat(0, q.capturedLength(0), marker_);
    }
    else if (const auto l = listRe.match(text); l.hasMatch())
    {
        setFormat(0, l.capturedLength(0), marker_);
    }

    auto each = [&](const QRegularExpression &re, const QTextCharFormat &f)
    {
        auto it = re.globalMatch(text);
        while (it.hasNext())
        {
            const auto m = it.next();
            paint(m.capturedStart(), m.capturedLength(), f);
        }
    };

    // i hate regex
    static const QRegularExpression strongRe(R"((\*\*|__)(?=\S)(.+?)(?<=\S)\1)");
    static const QRegularExpression emStarRe(R"((?<![\w*])\*(?![\s*])([^*]+?)(?<!\s)\*(?!\*))");
    static const QRegularExpression emUnderRe(R"((?<![\w_])_(?![\s_])([^_]+?)(?<!\s)_(?![\w_]))");
    static const QRegularExpression delRe(R"(~~(?=\S)(.+?)(?<=\S)~~)");
    static const QRegularExpression linkRe(R"(!?\[[^\]\n]*\]\([^)\n]*\)|<https?://[^>\s]+>|https?://[^\s)>]+)");
    static const QRegularExpression codeRe(R"((`+)(?!`)(.+?)(?<!`)\1(?!`))");

    each(strongRe, strong_);
    each(emStarRe, em_);
    each(emUnderRe, em_);
    each(delRe, del_);
    each(linkRe, link_);

    auto it = codeRe.globalMatch(text);
    while (it.hasNext())
    {
        const auto m = it.next();
        setFormat(m.capturedStart(), m.capturedLength(), code_);
    }
}