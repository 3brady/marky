#include "render/Renderer.h"
#include <QColor>
#include <QFont>
#include <QGuiApplication>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <algorithm>

namespace render
{
    namespace
    {

        struct Ctx
        {
            QTextCursor cur;
            bool firstBlock = true;
        };

        void startBlock(Ctx &c, const QTextBlockFormat &bf, const QTextCharFormat &cf)
        {
            if (c.firstBlock)
            {
                c.cur.setBlockFormat(bf);
                c.cur.setBlockCharFormat(cf);
                c.firstBlock = false;
            }
            else
            {
                c.cur.insertBlock(bf, cf);
            }
        }

        void inlines(Ctx &c, const md::Node &n, QTextCharFormat fmt)
        {
            switch (n.type)
            {
            case md::NodeType::Text:
                c.cur.insertText(QString::fromUtf8(n.text.data(), (qsizetype)n.text.size()), fmt);
                break; 
            case md::NodeType::LineBreak:
                c.cur.insertText(QString(QChar::LineSeparator), fmt);
                return;
            case md::NodeType::Strong:
                fmt.setFontWeight(QFont::Bold);
                break;
            case md::NodeType::Emphasis:
                fmt.setFontItalic(true);
                break;
            case md::NodeType::StrikeThrough:
                fmt.setFontStrikeOut(true);
                break;
            case md::NodeType::InlineCode:
                fmt.setFontFamilies({"monospace"});
                fmt.setBackground(QColor(0, 0, 0, 25));
                break;
            default:
                break;
            }
            for (const auto &ch : n.children)
                inlines(c, *ch, fmt);
        }

        void block(Ctx &c, const md::Node &n)
        {
            switch (n.type)
            {
            case md::NodeType::Heading:
            {
                static const double scale[] = {2.0, 1.6, 1.3, 1.15, 1.0, 0.9};
                const int lvl = std::clamp(n.level, 1, 6);

                QTextCharFormat cf;
                cf.setFontWeight(QFont::Bold);
                cf.setFontPointSize(QGuiApplication::font().pointSizeF() * scale[lvl - 1]);

                QTextBlockFormat bf;
                bf.setTopMargin(12);
                bf.setBottomMargin(6);
                bf.setHeadingLevel(lvl);

                startBlock(c, bf, cf);
                for (const auto &ch : n.children)
                    inlines(c, *ch, cf);
                break;
            }
            case md::NodeType::Paragraph:
            {
                QTextBlockFormat bf;
                bf.setBottomMargin(8);
                QTextCharFormat cf;
                startBlock(c, bf, cf);
                for (const auto &ch : n.children)
                    inlines(c, *ch, cf);
                break;
            }
            default:
                for (const auto &ch : n.children)
                    block(c, *ch);
                break;
            }
        }

    }

    std::unique_ptr<QTextDocument> toDocument(const md::Node &root)
    {
        auto doc = std::make_unique<QTextDocument>();
        Ctx c{QTextCursor(doc.get())};
        block(c, root);
        return doc;
    }

} // namespace render