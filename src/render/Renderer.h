#pragma once
#include <memory>
#include <vector>
#include <QString>
#include <QTextDocument>
#include "markdown/AST.h"

namespace render
{

    struct Anchor
    {
        int line;
        int position;
    };

    struct Result
    {
        std::unique_ptr<QTextDocument> doc;
        std::vector<Anchor> anchors;
    };

    Result toDocument(const md::Node &root, const QString &baseDir, int maxImageWidth);

}