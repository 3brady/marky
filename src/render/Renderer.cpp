#include "render/Renderer.h"
#include <QColor>
#include <QFont>
#include <QGuiApplication>
#include <QPalette>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextList>
#include <QTextTable>
#include <QDir>
#include <QImage>
#include <QTextFormat>
#include <QUrl>
#include <QTextDocumentFragment>
#include <algorithm>
#include <vector>

namespace render
{
    namespace
    {

        using md::Node;
        using md::NodeType;

        struct Ctx
        {
            QTextCursor cur;
            bool reuse = true; // true = the current block is empty and can be reused
            int listDepth = 0;
            int quoteDepth = 0;
            QTextDocument *doc = nullptr;
            QString baseDir;
            int maxImageWidth = 700;
        };

        QString str(const std::string &s)
        {
            return QString::fromUtf8(s.data(), (qsizetype)s.size());
        }

        bool isInline(NodeType t)
        {
            switch (t)
            {
            case NodeType::Text:
            case NodeType::LineBreak:
            case NodeType::Emphasis:
            case NodeType::Strong:
            case NodeType::StrikeThrough:
            case NodeType::InlineCode:
                return true;
            default:
                return false;
            }
        }

        QTextBlockFormat baseBlock(const Ctx &c)
        {
            QTextBlockFormat bf;
            if (c.quoteDepth > 0)
            {
                bf.setLeftMargin(16 * c.quoteDepth);
                bf.setBackground(QColor(128, 128, 128, 28));
            }
            return bf;
        }

        QTextCharFormat baseChar(const Ctx &c)
        {
            QTextCharFormat cf;
            if (c.quoteDepth > 0)
                cf.setForeground(QGuiApplication::palette().color(QPalette::PlaceholderText));
            return cf;
        }

        void startBlock(Ctx &c, const QTextBlockFormat &bf, const QTextCharFormat &cf)
        {
            if (c.reuse)
            {
                c.cur.setBlockFormat(bf);
                c.cur.setBlockCharFormat(cf);
                c.reuse = false;
            }
            else
            {
                c.cur.insertBlock(bf, cf);
            }
        }

        QString plainText(const Node &n)
        {
            QString s = str(n.text);
            for (const auto &ch : n.children)
                s += plainText(*ch);
            return s;
        }

        void insertImage(Ctx &c, const Node &n, const QTextCharFormat &fmt)
        {
            const QString src = str(n.url);
            QString alt = plainText(n);
            if (alt.isEmpty())
                alt = src;

            const bool remote = src.startsWith("http://") || src.startsWith("https://") || src.startsWith("data:");
            QImage img;
            QString path;
            if (!remote)
            {
                path = QDir(c.baseDir).absoluteFilePath(QUrl::fromPercentEncoding(src.toUtf8()));
                img.load(path);
            }
            if (img.isNull())
            {
                c.cur.insertText("[" + alt + "]", fmt);
                return;
            }

            const QUrl key = QUrl::fromLocalFile(path);
            c.doc->addResource(QTextDocument::ImageResource, key, img);

            QTextImageFormat f;
            f.merge(fmt);
            f.setName(key.toString());
            const int w = qMin(img.width(), c.maxImageWidth);
            f.setWidth(w);
            f.setHeight(img.height() * double(w) / img.width());
            if (!n.title.empty())
                f.setToolTip(str(n.title));
            c.cur.insertImage(f);
        }

        void inlines(Ctx &c, const Node &n, QTextCharFormat fmt)
        {
            switch (n.type)
            {
            case NodeType::Text:
                c.cur.insertText(str(n.text), fmt);
                break;
            case NodeType::LineBreak:
                c.cur.insertText(QString(QChar::LineSeparator), fmt);
                return;
            case NodeType::Strong:
                fmt.setFontWeight(QFont::Bold);
                break;
            case NodeType::Emphasis:
                fmt.setFontItalic(true);
                break;
            case NodeType::StrikeThrough:
                fmt.setFontStrikeOut(true);
                break;
            case NodeType::InlineCode:
                fmt.setFontFamilies({"monospace"});
                fmt.setBackground(QColor(0, 0, 0, 25));
                break;
            case NodeType::Link:
                fmt.setAnchor(true);
                fmt.setAnchorHref(str(n.url));
                fmt.setForeground(QGuiApplication::palette().color(QPalette::Link));
                fmt.setFontUnderline(true);
                if (!n.title.empty())
                    fmt.setToolTip(str(n.title));
                break;
            case NodeType::Image:
                insertImage(c, n, fmt);
                return;
            case NodeType::Entity:
            {
                const QString raw = str(n.text);
                const QString decoded = QTextDocumentFragment::fromHtml(raw).toPlainText();
                c.cur.insertText(decoded.isEmpty() ? raw : decoded, fmt);
                return;
            }
            default:
                break;
            }
            for (const auto &ch : n.children)
                inlines(c, *ch, fmt);
        }

        void block(Ctx &c, const Node &n);

        void listBlock(Ctx &c, const Node &n)
        {
            static const QTextListFormat::Style bullets[] = {
                QTextListFormat::ListDisc, QTextListFormat::ListCircle, QTextListFormat::ListSquare};

            QTextListFormat lf;
            lf.setStyle(n.type == NodeType::OrderedList ? QTextListFormat::ListDecimal
                                                        : bullets[c.listDepth % 3]);
            lf.setIndent(c.listDepth + 1);

            QTextList *list = nullptr;
            c.listDepth++;
            for (const auto &item : n.children)
            {
                QTextBlockFormat bf = baseBlock(c);
                bf.setTopMargin(0);
                bf.setBottomMargin(2);
                if (item->task)
                {
                    if (item->task == 2)
                    {
                        bf.setMarker(QTextBlockFormat::MarkerType::Checked);
                    }
                    else
                    {
                        bf.setMarker(QTextBlockFormat::MarkerType::Unchecked);
                    }
                }
                const QTextCharFormat cf = baseChar(c);

                startBlock(c, bf, cf);
                if (!list)
                    list = c.cur.createList(lf);
                else
                    list->add(c.cur.block());

                bool fresh = true;
                for (const auto &ch : item->children)
                {
                    if (isInline(ch->type))
                    {
                        inlines(c, *ch, cf);
                        fresh = false;
                    }
                    else if (ch->type == NodeType::Paragraph && fresh)
                    {
                        for (const auto &g : ch->children)
                            inlines(c, *g, cf);
                        fresh = false;
                    }
                    else
                    {
                        block(c, *ch);
                        fresh = false;
                    }
                }
            }
            c.listDepth--;
            if (c.listDepth == 0)
            { // only after the outermost list
                QTextBlockFormat gap;
                gap.setBottomMargin(8);
                c.cur.mergeBlockFormat(gap);
            }
        }

        void tableBlock(Ctx &c, const Node &n)
        {
            std::vector<const Node *> rows;
            for (const auto &section : n.children)
                for (const auto &row : section->children)
                    rows.push_back(row.get());
            if (rows.empty() || rows[0]->children.empty())
                return;
            const int ncols = (int)rows[0]->children.size();

            QTextTableFormat tf;
            tf.setBorder(1);
            tf.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
            tf.setBorderBrush(QColor(128, 128, 128));
            tf.setCellPadding(6);
            tf.setCellSpacing(0);
            tf.setHeaderRowCount(1);
            tf.setWidth(QTextLength(QTextLength::PercentageLength, 100));
            tf.setBottomMargin(10);

            if (!c.reuse)
                c.cur.insertBlock(QTextBlockFormat(), QTextCharFormat());
            QTextTable *table = c.cur.insertTable((int)rows.size(), ncols, tf);

            for (int r = 0; r < (int)rows.size(); ++r)
            {
                const auto &cells = rows[r]->children;
                for (int col = 0; col < ncols && col < (int)cells.size(); ++col)
                {
                    QTextTableCell cell = table->cellAt(r, col);
                    QTextCharFormat cf;
                    if (cells[col]->type == NodeType::TableHeaderCell)
                    {
                        cf.setFontWeight(QFont::Bold);
                        QTextTableCellFormat tcf;
                        tcf.setBackground(QColor(128, 128, 128, 40));
                        cell.setFormat(tcf);
                    }
                    Ctx cc = c;
                    cc.cur = cell.firstCursorPosition();
                    for (const auto &ch : cells[col]->children)
                        inlines(cc, *ch, cf);
                }
            }

            c.cur.movePosition(QTextCursor::End); // step out of the table

            QTextBlockFormat tinyBlock;
            tinyBlock.setTopMargin(0);
            tinyBlock.setBottomMargin(0);
            QTextCharFormat tinyChar;
            tinyChar.setFontPointSize(1);
            c.cur.setBlockFormat(tinyBlock);
            c.cur.setBlockCharFormat(tinyChar);
            c.reuse = false;
        }

        void block(Ctx &c, const Node &n)
        {
            switch (n.type)
            {
            case NodeType::Heading:
            {
                static const double scale[] = {2.0, 1.6, 1.3, 1.15, 1.0, 0.9};
                const int lvl = std::clamp(n.level, 1, 6);

                QTextCharFormat cf = baseChar(c);
                cf.setFontWeight(QFont::Bold);
                cf.setFontPointSize(QGuiApplication::font().pointSizeF() * scale[lvl - 1]);

                QTextBlockFormat bf = baseBlock(c);
                bf.setTopMargin(12);
                bf.setBottomMargin(6);
                bf.setHeadingLevel(lvl);

                startBlock(c, bf, cf);
                for (const auto &ch : n.children)
                    inlines(c, *ch, cf);
                break;
            }
            case NodeType::Paragraph:
            {
                QTextBlockFormat bf = baseBlock(c);
                bf.setBottomMargin(8);
                const QTextCharFormat cf = baseChar(c);
                startBlock(c, bf, cf);
                for (const auto &ch : n.children)
                    inlines(c, *ch, cf);
                break;
            }
            case NodeType::BulletList:
            case NodeType::OrderedList:
                listBlock(c, n);
                break;
            case NodeType::Quote:
                c.quoteDepth++;
                for (const auto &ch : n.children)
                    block(c, *ch);
                c.quoteDepth--;
                break;
            case NodeType::CodeBlock:
            {
                QString code;
                for (const auto &ch : n.children)
                    code += str(ch->text);
                while (code.endsWith('\n'))
                    code.chop(1);
                code.replace(QLatin1Char('\n'), QChar(QChar::LineSeparator));

                QTextBlockFormat bf = baseBlock(c);
                bf.setBackground(QColor(0, 0, 0, 30));
                bf.setTopMargin(4);
                bf.setBottomMargin(10);
                QTextCharFormat cf = baseChar(c);
                cf.setFontFamilies({"monospace"});

                startBlock(c, bf, cf);
                c.cur.insertText(code, cf);
                break;
            }
            case NodeType::Table:
                tableBlock(c, n);
                break;
            case NodeType::Rule:
            {
                QTextBlockFormat bf = baseBlock(c);
                bf.setProperty(QTextFormat::BlockTrailingHorizontalRulerWidth,
                               QTextLength(QTextLength::PercentageLength, 100));
                startBlock(c, bf, QTextCharFormat());
                break;
            }
            default:
                for (const auto &ch : n.children)
                    block(c, *ch);
                break;
            }
        }

    }

    std::unique_ptr<QTextDocument> toDocument(const md::Node &root, const QString &baseDir, int maxImageWidth)
    {
        auto doc = std::make_unique<QTextDocument>();
        Ctx c;
        c.cur = QTextCursor(doc.get());
        c.doc = doc.get();
        c.baseDir = baseDir;
        c.maxImageWidth = maxImageWidth;
        block(c, root);
        return doc;
    }

} // namespace render