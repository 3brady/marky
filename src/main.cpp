#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <cstdio>
#include "app/MainWindow.h"
#include "markdown/AST.h"
#include "markdown/Parser.h"

static void dump(const md::Node &n, int depth)
{
    std::printf("%*s", depth * 2, "");

    std::printf("%s", md::nodeName(n.type));
    if (n.type == md::NodeType::Heading)
        std::printf(" level=%d", n.level);
    else if (n.type == md::NodeType::Text)
        std::printf(" \"%s\"", n.text.c_str());
    std::printf("\n");

    for (const auto &child : n.children)
    {
        dump(*child, depth + 1);
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    const QStringList args = app.arguments();
    const QString dir = args.size() > 1 ? args.at(1) : QDir::currentPath();

    QFileInfo info(dir);
    if (!info.isDir())
    {
        std::fprintf(stderr, "marky: '%s' is not a directory\n", qPrintable(dir));
        return 1;
    }

    auto root = md::parse("# Hi *there*\n\nsome **bold** and `code`\n");
    if (root)
    {
        dump(*root, 0);
    }

    MainWindow w(info.absoluteFilePath());
    w.resize(1300, 700);
    w.show();
    return app.exec();
}