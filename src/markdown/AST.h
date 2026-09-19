#pragma once
#include <memory>
#include <vector>

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
        TableCell
    };

    struct Node
    {
        NodeType type;
        int level = 0;
        std::vector<std::unique_ptr<Node>> children;

        explicit Node(NodeType t) : type(t) {}
    };

} // namespace md