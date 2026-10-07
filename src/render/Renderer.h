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

    struct Options{
        QString baseDir;
        int maxImageWidth = 700;
        double fontPointSize = 0;
    };

    Result toDocument(const md::Node &root, const Options &opt);

}