#pragma once
#include <memory>
#include <QTextDocument>
#include "markdown/AST.h"

namespace render
{
    std::unique_ptr<QTextDocument> toDocument(const md::Node &root);
}