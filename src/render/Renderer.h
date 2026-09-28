#pragma once
#include <memory>
#include <QString>
#include <QTextDocument>
#include "markdown/AST.h"

namespace render
{
    std::unique_ptr<QTextDocument> toDocument(const md::Node &root);
    std::unique_ptr<QTextDocument> toDocument(const md::Node &root , const QString& baseDir , int maxImageWidth);
}