#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <cstdio>
#include "app/MainWindow.h"
#include "style/Theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setStyle("Fusion");

    QCoreApplication::setOrganizationName("marky");
    QCoreApplication::setApplicationName("marky");

    QSettings settings;
    theme::apply(settings.value("dark", theme::systemPrefersDark()).toBool());

    bool useLast = false;
    QString target;
    const QStringList args = app.arguments().mid(1);
    for (const QString &a : args)
    {
        if (a == "--last")
            useLast = true;
        else if (target.isEmpty())
            target = a;
    }

    if (target.isEmpty())
    {
        if (useLast)
        {
            target = settings.value("lastProject", QDir::homePath()).toString();
            if (!QFileInfo(target).isDir())
                target = QDir::homePath();
        }
        else
        {
            target = QDir::currentPath();
        }
    }

    const QFileInfo info(target);
    if (!info.exists())
    {
        std::fprintf(stderr, "marky: '%s' does not exist\n", qPrintable(target));
        return 1;
    }

    QString projectDir;
    QString fileToOpen;
    if (info.isDir())
    {
        projectDir = info.absoluteFilePath();
    }
    else
    {
        projectDir = info.absolutePath();
        fileToOpen = info.canonicalFilePath();
    }

    settings.setValue("lastProject", projectDir);

    MainWindow w(projectDir);
    w.resize(1300, 700);
    w.show();
    if (!fileToOpen.isEmpty())
        w.openFile(fileToOpen);
    return app.exec();
}