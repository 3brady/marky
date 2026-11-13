#include "markdown/Parser.h"
#include <md4c.h>
#include <cstdio>
#include <algorithm>
#include "AST.h"

namespace md
{

    NodeType toNodeType(MD_BLOCKTYPE t)
    {

        switch (t)
        {
        case MD_BLOCK_DOC:
            return NodeType::Document;

        case MD_BLOCK_QUOTE:
            return NodeType::Quote;

        case MD_BLOCK_UL:
            return NodeType::BulletList;

        case MD_BLOCK_OL:
            return NodeType::OrderedList;

        case MD_BLOCK_LI:
            return NodeType::ListItem;

        case MD_BLOCK_HR:
            return NodeType::Rule;

        case MD_BLOCK_H:
            return NodeType::Heading;

        case MD_BLOCK_CODE:
            return NodeType::CodeBlock;

        case MD_BLOCK_HTML:
            return NodeType::Html;

        case MD_BLOCK_P:
            return NodeType::Paragraph;

        case MD_BLOCK_TABLE:
            return NodeType::Table;

        case MD_BLOCK_THEAD:
            return NodeType::TableHead;

        case MD_BLOCK_TBODY:
            return NodeType::TableBody;

        case MD_BLOCK_TR:
            return NodeType::TableRow;

        case MD_BLOCK_TH:
            return NodeType::TableHeaderCell;

        case MD_BLOCK_TD:
            return NodeType::TableCell;
        }
        return NodeType::Paragraph;
    }

    struct ASTState
    {
        std::unique_ptr<Node> root;
        std::vector<Node *> stack;
        const char *base = nullptr;
        size_t size = 0;
        std::vector<size_t> lineStarts;
        std::vector<Node *> pending;
        size_t lastLine = 0;
    };

    static int lineOf(const ASTState &s, const char *p)
    {
        if (p < s.base || p >= s.base + s.size)
            return -1;

        const size_t off = size_t(p - s.base);
        return int(std::upper_bound(s.lineStarts.begin(), s.lineStarts.end(), off) - s.lineStarts.begin()) - 1;
    }

    static int onEnterNode(MD_BLOCKTYPE type, void *detail, void *user)
    {

        auto *s = static_cast<ASTState *>(user);
        auto node = std::make_unique<Node>(toNodeType(type));

        if (type == MD_BLOCK_H)
        {
            node->level = static_cast<const MD_BLOCK_H_DETAIL *>(detail)->level;
        }

        if (type == MD_BLOCK_LI)
        {
            auto *d = static_cast<const MD_BLOCK_LI_DETAIL *>(detail);
            if (d->is_task)
            {
                node->task = (d->task_mark == ' ') ? 1 : 2;
            }
        }

        if (type == MD_BLOCK_HR)
        {
            for (size_t l = s->lastLine; l < s->lineStarts.size(); ++l)
            {
                size_t start = s->lineStarts[l];
                size_t end = (l + 1 < s->lineStarts.size()) ? s->lineStarts[l + 1] - 1 : s->size;
                int count = 0;
                char ruleChar = 0;
                for (size_t i = start; i < end; ++i)
                {
                    char c = s->base[i];
                    if (c == '-' || c == '*' || c == '_')
                    {
                        if (!ruleChar) ruleChar = c;
                        if (c == ruleChar) count++;
                    }
                }
                if (count >= 3)
                {
                    node->line = int(l);
                    s->lastLine = l;
                    break;
                }
            }
        }

        Node *raw = node.get();

        if (s->stack.empty())
        {
            s->root = std::move(node);
        }
        else
        {
            s->stack.back()->children.push_back(std::move(node));
        }

        s->stack.push_back(raw);
        if (raw == s->root.get())
        {
            raw->line = 0;
        }
        else if (raw->line < 0)
        {
            s->pending.push_back(raw);
        }
        return 0;
    }

    static int onLeaveNode(MD_BLOCKTYPE, void *, void *user)
    {
        static_cast<ASTState *>(user)->stack.pop_back();
        return 0;
    }

    static NodeType toNodeType(MD_SPANTYPE t)
    {
        switch (t)
        {
        case MD_SPAN_EM:
            return NodeType::Emphasis;
        case MD_SPAN_STRONG:
            return NodeType::Strong;
        case MD_SPAN_DEL:
            return NodeType::StrikeThrough;
        case MD_SPAN_CODE:
            return NodeType::InlineCode;
        case MD_SPAN_A:
            return NodeType::Link;
        case MD_SPAN_IMG:
            return NodeType::Image;
        default:
            return NodeType::Text;
        }
    }

    static std::string attr(const MD_ATTRIBUTE &a)
    {
        return a.text ? std::string(a.text, a.size) : std::string();
    }

    static int onEnterSpanNode(MD_SPANTYPE type, void *detail, void *user)
    {
        auto *s = static_cast<ASTState *>(user);

        auto node = std::make_unique<Node>(toNodeType(type));
        Node *raw = node.get();

        if (type == MD_SPAN_A)
        {
            auto *d = static_cast<const MD_SPAN_A_DETAIL *>(detail);
            node->url = attr(d->href);
            node->title = attr(d->title);
        }

        if (type == MD_SPAN_IMG)
        {
            auto *d = static_cast<const MD_SPAN_IMG_DETAIL *>(detail);
            node->url = attr(d->src);
            node->title = attr(d->title);
        }

        s->stack.back()->children.push_back(std::move(node));
        s->stack.push_back(raw);
        return 0;
    }

    static int onLeaveSpanNode(MD_SPANTYPE, void *, void *user)
    {
        static_cast<ASTState *>(user)->stack.pop_back();
        return 0;
    }

    static int onTextNode(MD_TEXTTYPE type, const MD_CHAR *str, MD_SIZE size, void *user)
    {
        auto *s = static_cast<ASTState *>(user);

        auto node = std::make_unique<Node>(NodeType::Text);
        switch (type)
        {
        case MD_TEXT_BR:
            node->type = NodeType::LineBreak;
            break;
        case MD_TEXT_SOFTBR:
            node->text = " ";
            break;
        case MD_TEXT_NULLCHAR:
            node->text = "\xEF\xBF\xBD";
            break; // U+FFFD
        case MD_TEXT_ENTITY:
            node->type = NodeType::Entity;
            node->text.assign(str, size); // raw, e.g. "&copy;"
            break;
        default:
            node->text.assign(str, size);
            break;
        }

        if ((type == MD_TEXT_NORMAL || type == MD_TEXT_CODE || type == MD_TEXT_HTML || type == MD_TEXT_ENTITY) && !s->pending.empty())
        {
            const int ln = lineOf(*s, str);
            if (ln >= 0)
            {
                s->lastLine = size_t(ln);
                for (Node *b : s->pending)
                {
                    b->line = ln;
                }
                s->pending.clear();
            }
        }

        s->stack.back()->children.push_back(std::move(node));
        return 0;
    }

    std::unique_ptr<Node> parse(std::string_view text)
    {

        ASTState state;

        MD_PARSER parser = {};
        parser.abi_version = 0;
        parser.flags = MD_DIALECT_GITHUB;
        parser.enter_block = onEnterNode;
        parser.leave_block = onLeaveNode;
        parser.enter_span = onEnterSpanNode;
        parser.leave_span = onLeaveSpanNode;
        parser.text = onTextNode;
        state.base = text.data();
        state.size = text.size();
        state.lineStarts.push_back(0);

        for (size_t i = 0; i < text.size(); i++)
        {
            if (text[i] == '\n')
            {
                state.lineStarts.push_back(i + 1);
            }
        }

        if (md_parse(text.data(), (MD_SIZE)text.size(), &parser, &state) != 0)
        {
            return nullptr;
        }

        return std::move(state.root);
    }

} // namespace md