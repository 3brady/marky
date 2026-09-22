#pragma once
#include <memory>
#include <vector>
#include <string>

namespace md
{

    enum class NodeType
    {
        Document,
        Quote,
        BulletList,
        OrderedList,
        ListItem,
        Rule,
        Heading,
        CodeBlock,
        Html,
        Paragraph,
        Table,
        TableHead,
        TableBody,
        TableRow,
        TableHeaderCell,
        TableCell,
        Text,
        LineBreak,
        Emphasis,
        Strong,
        StrikeThrough,
        InlineCode
    };

    inline const char *nodeName(NodeType t)
    {
        switch (t)
        {
        case NodeType::Document:
            return "Document";
        case NodeType::Quote:
            return "Quote";
        case NodeType::BulletList:
            return "BulletList";
        case NodeType::OrderedList:
            return "OrderedList";
        case NodeType::ListItem:
            return "ListItem";
        case NodeType::Rule:
            return "Rule";
        case NodeType::Heading:
            return "Heading";
        case NodeType::CodeBlock:
            return "CodeBlock";
        case NodeType::Html:
            return "Html";
        case NodeType::Paragraph:
            return "Paragraph";
        case NodeType::Table:
            return "Table";
        case NodeType::TableHead:
            return "TableHead";
        case NodeType::TableBody:
            return "TableBody";
        case NodeType::TableRow:
            return "TableRow";
        case NodeType::TableHeaderCell:
            return "TableHeaderCell";
        case NodeType::TableCell:
            return "TableCell";
        case NodeType::Text:
            return "Text";
        case NodeType::LineBreak:
            return "LineBreak";
        case NodeType::Emphasis:
            return "Emphasis";
        case NodeType::Strong:
            return "Strong";
        case NodeType::StrikeThrough:
            return "StrikeThrough";
        case NodeType::InlineCode:
            return "InlineCode";
        }
        return "?";
    }

    struct Node
    {
        NodeType type;
        int level = 0;
        std::string text;
        std::vector<std::unique_ptr<Node>> children;

        explicit Node(NodeType t) : type(t) {}
    };

} // namespace md