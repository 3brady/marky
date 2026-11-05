// Parser tests: markdown text in -> expected tree out.
// No test framework needed. Run with:  ctest --test-dir build --output-on-failure
//
// A tree is written in one line:  Name, then details, then (children).
//   Heading2              heading level 2
//   Text"hi"              text node
//   Link<url 'title'>     link (title only if present)
//   ListItem[x] / [ ]     task list item, checked / unchecked
//   Name@3                (line mode only) block starts on source line 3

#include "markdown/Parser.h"

#include <cstdio>
#include <string>
#include <string_view>

namespace {

int g_run = 0;
int g_failed = 0;

std::string esc(std::string_view s) {
    std::string out;
    for (char c : s) {
        if (c == '\n')      out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\0') out += "\\0";
        else                out += c;
    }
    return out;
}

std::string show(const md::Node& n, bool lines) {
    using md::NodeType;
    std::string s = md::nodeName(n.type);
    if (n.type == NodeType::Heading) s += std::to_string(n.level);
    if (n.type == NodeType::Text || n.type == NodeType::Entity) s += "\"" + esc(n.text) + "\"";
    if (n.type == NodeType::Link || n.type == NodeType::Image) {
        s += "<" + n.url;
        if (!n.title.empty()) s += " '" + n.title + "'";
        s += ">";
    }
    if (n.task) s += (n.task == 2) ? "[x]" : "[ ]";
    if (lines && n.line >= 0) s += "@" + std::to_string(n.line + 1);
    if (!n.children.empty()) {
        s += "(";
        bool first = true;
        for (const auto& c : n.children) {
            if (!first) s += " ";
            first = false;
            s += show(*c, lines);
        }
        s += ")";
    }
    return s;
}

void fail(const char* name, const std::string& detail) {
    ++g_failed;
    std::printf("FAIL: %s\n%s\n", name, detail.c_str());
}

void expectTree(const char* name, std::string_view markdown, const std::string& expected,
                bool lines = false) {
    ++g_run;
    auto root = md::parse(markdown);
    const std::string actual = root ? show(*root, lines) : "<null>";
    if (actual != expected)
        fail(name, "  input:    " + esc(markdown) + "\n  expected: " + expected +
                   "\n  actual:   " + actual);
}

void expectTrue(const char* name, bool condition, const std::string& detail = "") {
    ++g_run;
    if (!condition) fail(name, "  " + detail);
}

} // namespace

int main() {
    // ---------- blocks ----------
    expectTree("empty input", "", "Document");
    expectTree("headings", "# A\n## B\n",
               R"(Document(Heading1(Text"A") Heading2(Text"B")))");
    expectTree("setext heading", "Setext\n======\n",
               R"(Document(Heading1(Text"Setext")))");
    expectTree("rule between paragraphs", "text\n\n---\n\nmore\n",
               R"(Document(Paragraph(Text"text") Rule Paragraph(Text"more")))");
    expectTree("blockquote joins lines with a soft break", "> q\n> r\n",
               R"(Document(Quote(Paragraph(Text"q" Text" " Text"r"))))");
    expectTree("nested blockquote", "> a\n>> b\n",
               R"(Document(Quote(Paragraph(Text"a") Quote(Paragraph(Text"b")))))");
    expectTree("fenced code block", "```cpp\nint x;\nreturn 0;\n```\n",
               R"(Document(CodeBlock(Text"int x;" Text"\n" Text"return 0;" Text"\n")))");
    expectTree("indented code block", "    indented code\n",
               R"(Document(CodeBlock(Text"indented code" Text"\n")))");
    expectTree("html block", "<div>x</div>\n",
               R"(Document(Html(Text"<div>x</div>" Text"\n")))");

    // ---------- lists ----------
    expectTree("tight bullet list with nesting", "- a\n- b\n  - c\n",
               R"(Document(BulletList(ListItem(Text"a") ListItem(Text"b" BulletList(ListItem(Text"c"))))))");
    expectTree("ordered list", "1. x\n2. y\n",
               R"(Document(OrderedList(ListItem(Text"x") ListItem(Text"y"))))");
    expectTree("ordered list with nested bullets", "1. one\n   - nested\n2. two\n",
               R"(Document(OrderedList(ListItem(Text"one" BulletList(ListItem(Text"nested"))) ListItem(Text"two"))))");
    expectTree("loose list keeps paragraphs", "- a\n\n- b\n",
               R"(Document(BulletList(ListItem(Paragraph(Text"a")) ListItem(Paragraph(Text"b")))))");
    expectTree("task list", "- [x] done\n- [ ] open\n",
               R"(Document(BulletList(ListItem[x](Text"done") ListItem[ ](Text"open"))))");

    // ---------- tables ----------
    expectTree("table", "| a | b |\n|---|---|\n| 1 | 2 |\n",
               R"(Document(Table(TableHead(TableRow(TableHeaderCell(Text"a") TableHeaderCell(Text"b"))) TableBody(TableRow(TableCell(Text"1") TableCell(Text"2"))))))");

    // ---------- inline formatting ----------
    expectTree("bold, italic, code, strikethrough",
               "some **bold** and *it* and `code` and ~~gone~~\n",
               R"(Document(Paragraph(Text"some " Strong(Text"bold") Text" and " Emphasis(Text"it") Text" and " InlineCode(Text"code") Text" and " StrikeThrough(Text"gone"))))");
    expectTree("hard break and soft break", "line one  \nline two\nline three\n",
               R"(Document(Paragraph(Text"line one" LineBreak Text"line two" Text" " Text"line three")))");
    expectTree("entities stay raw for the renderer", "Fish &amp; chips &copy; &#9731;\n",
               R"(Document(Paragraph(Text"Fish " Entity"&amp;" Text" chips " Entity"&copy;" Text" " Entity"&#9731;")))");
    expectTree("utf-8 text is untouched", "caf\xC3\xA9 \xF0\x9F\x98\x80\n",
               "Document(Paragraph(Text\"caf\xC3\xA9 \xF0\x9F\x98\x80\"))");

    // ---------- links and images ----------
    expectTree("link with title, image, autolink",
               "[t](http://x.com \"tip\") ![alt](p.png) <https://c.ai>\n",
               R"(Document(Paragraph(Link<http://x.com 'tip'>(Text"t") Text" " Image<p.png>(Text"alt") Text" " Link<https://c.ai>(Text"https://c.ai"))))");
    expectTree("bold link", "**[bold link](https://e.com)**\n",
               R"(Document(Paragraph(Strong(Link<https://e.com>(Text"bold link")))))");
    expectTree("image inside a link (badge)", "[![badge](b.png)](https://e.com)\n",
               R"(Document(Paragraph(Link<https://e.com>(Image<b.png>(Text"badge")))))");
    expectTree("bare www and https urls become links",
               "www.example.com and https://example.org\n",
               R"(Document(Paragraph(Link<http://www.example.com>(Text"www.example.com") Text" and " Link<https://example.org>(Text"https://example.org"))))");

    // ---------- source lines (used by scroll sync) ----------
    expectTree("block start lines",
               "# T\n\n> q\n\n- a\n- b\n\n| a | b |\n|---|---|\n| 1 | 2 |\n",
               R"(Document@1(Heading1@1(Text"T") Quote@3(Paragraph@3(Text"q")) BulletList@5(ListItem@5(Text"a") ListItem@6(Text"b")) Table@8(TableHead@8(TableRow@8(TableHeaderCell@8(Text"a") TableHeaderCell@8(Text"b"))) TableBody@10(TableRow@10(TableCell@10(Text"1") TableCell@10(Text"2"))))))",
               true);
    expectTree("leading blank lines move the first block down", "\n\n# late\n",
               R"(Document@1(Heading1@3(Text"late")))", true);
    expectTree("loose list items have their own lines", "- a\n\n- b\n",
               R"(Document@1(BulletList@1(ListItem@1(Paragraph@1(Text"a")) ListItem@3(Paragraph@3(Text"b")))))",
               true);
    expectTree("entity paragraph line tracking", "&copy;\n",
               R"(Document@1(Paragraph@1(Entity"&copy;")))", true);
    expectTree("rule line tracking", "text\n\n---\n\nmore\n",
               R"(Document@1(Paragraph@1(Text"text") Rule@3 Paragraph@5(Text"more")))", true);
    expectTree("rule at end of input line tracking", "text\n\n---\n",
               R"(Document@1(Paragraph@1(Text"text") Rule@3))", true);

    // ---------- robustness: must never crash or return null ----------
    {
        auto root = md::parse(std::string_view("a\0b", 3));
        expectTrue("embedded NUL byte", root != nullptr);
        if (root) {
            const std::string s = show(*root, false);
            expectTrue("NUL becomes U+FFFD", s.find("\xEF\xBF\xBD") != std::string::npos, s);
        }
    }
    expectTrue("invalid utf-8", md::parse("\xFF\xFE broken\n") != nullptr);
    expectTrue("no trailing newline", md::parse("# Title") != nullptr);
    expectTree("CRLF line endings", "# A\r\n\r\ntext\r\n",
               R"(Document(Heading1(Text"A") Paragraph(Text"text")))");
    expectTrue("deeply nested quotes", md::parse(std::string(300, '>') + " x\n") != nullptr);
    {
        std::string big;
        for (int i = 0; i < 20000; ++i) big += "paragraph " + std::to_string(i) + "\n\n";
        auto root = md::parse(big);
        expectTrue("20000 paragraphs", root && root->children.size() == 20000,
                   root ? std::to_string(root->children.size()) + " children" : "null");
    }

    std::printf("%d checks, %d failed\n", g_run, g_failed);
    return g_failed == 0 ? 0 : 1;
}