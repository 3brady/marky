#pragma once
#include <string_view>
#include <memory>
#include "markdown/AST.h"

namespace md
{
    std::unique_ptr<Node> parse(std::string_view text);
}