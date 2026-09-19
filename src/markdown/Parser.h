#pragma once
#include <string_view>
#include <memory>
#include "markdown/AST.h"

namespace md
{
    bool dumpBlocks(std::string_view text);
    std::unique_ptr<Node> parse(std::string_view text);
}